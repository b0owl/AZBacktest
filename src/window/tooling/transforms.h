#pragma once

#include "imgui.h"
#include "imgui_internal.h" // ImGuiSettingsHandler / ImHashStr for ini persistence
#include "implot.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "formula.h"
#include "seriesPool.h"

namespace transformManagement {

/// @brief the numbers shown before and after
struct Stats {
    int   n    = 0;
    double mean = 0, median = 0, stdev = 0;
    double min  = 0, max    = 0, sum   = 0;
    double p25  = 0, p75    = 0;
};

/// @brief percentile of an already sorted vector
/// @param pct 0..100, clamped
inline double percentileOf(const std::vector<double>& sorted, double pct) {
    if (sorted.empty()) return 0.0;
    if (pct <= 0.0)   return sorted.front();
    if (pct >= 100.0) return sorted.back();
    const double pos = (pct / 100.0) * (double)(sorted.size() - 1);
    const size_t lo  = (size_t)pos;
    const size_t hi  = lo + 1 < sorted.size() ? lo + 1 : lo;
    return sorted[lo] + (pos - (double)lo) * ((double)sorted[hi] - (double)sorted[lo]);
}

/// @brief stats for a column, stdev is the sample one (n-1)
inline Stats computeStats(const std::vector<double>& v) {
    Stats st;
    st.n = (int)v.size();
    if (st.n == 0) return st;

    for (double x : v) st.sum += x;
    st.mean = st.sum / (double)st.n;

    double acc = 0.0;
    for (double x : v) { const double d = (double)x - st.mean; acc += d * d; }
    st.stdev = st.n > 1 ? std::sqrt(acc / (double)(st.n - 1)) : 0.0;

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    st.min    = sorted.front();
    st.max    = sorted.back();
    st.median = percentileOf(sorted, 50.0);
    st.p25    = percentileOf(sorted, 25.0);
    st.p75    = percentileOf(sorted, 75.0);
    return st;
}

/// @brief one window, a source column plus the two formulas over it
struct TransformWindow {
    std::string id;
    int  selectedSeriesIdx = -1;
    int  col               = 0;
    char valueExpr[256]    = "x";   ///< new value per point
    char keepExpr[256]     = "";    ///< optional filter, point kept when nonzero
    char outName[64]       = "";    ///< name typed into the save box
    char publishedName[64] = "";    ///< pool series this transform owns, empty = not saved
    bool showSource        = true;  ///< draw the untransformed series behind the result
};

/// @brief every open transform window
inline std::vector<TransformWindow> transforms;

/// @brief find a transform window by id, or nullptr
inline TransformWindow* findTransform(const std::string& id) {
    for (auto& t : transforms) if (t.id == id) return &t;
    return nullptr;
}

/// @brief register a new transform window; no-op if `id` already exists
inline void newTransform(std::string id) {
    if (findTransform(id) == nullptr) {
        transforms.push_back(TransformWindow{});
        transforms.back().id = std::move(id);
    }
}

/// @brief lowest free id, so new windows don't clash with restored ones
inline int nextTransformId() {
    int maxId = -1;
    for (auto& t : transforms) {
        char* endp = nullptr;
        long v = std::strtol(t.id.c_str(), &endp, 10);
        if (endp != t.id.c_str() && *endp == '\0' && v > maxId) maxId = (int)v;
    }
    return maxId + 1;
}

/// what p() needs to answer, a sorted copy of the column being worked on
struct EvalCtx {
    const std::vector<double>* sorted = nullptr;
};

/// @brief p(k), any percentile of the source column, k is 0..100 and gets clamped
/// this is a host function rather than a variable so you aren't stuck with
/// whichever few we thought to precompute
inline double percentileFn(void* ud, const std::string& name,
                           const std::vector<double>& args,
                           bool& handled, std::string& err) {
    if (name != "p") return 0.0;   // leave handled false, it isn't ours
    handled = true;

    if (args.size() != 1) {
        err = "p() takes 1 argument, got " + std::to_string(args.size());
        return 0.0;
    }
    const EvalCtx* c = (const EvalCtx*)ud;
    if (!c || !c->sorted || c->sorted->empty()) { err = "p() has nothing to work on"; return 0.0; }
    return percentileOf(*c->sorted, args[0]);
}

/// @brief names a formula can use, x and i get rewritten per point
inline std::vector<formula::Var> buildVars(const Stats& st) {
    return {
        {"x", 0.0}, {"i", 0.0},
        {"n", (double)st.n},
        {"mean", st.mean}, {"median", st.median}, {"stdev", st.stdev},
        {"min", st.min},   {"max", st.max},       {"sum", st.sum},
        {"p25", st.p25},   {"p75", st.p75},
    };
}

/// @brief run both formulas over a column
/// @param valueExpr per point value, blank means just x
/// @param keepExpr  optional filter, point survives when nonzero
/// @param err       first error hit, cleared on success
/// @return the new column, empty if a formula didn't parse
inline std::vector<double> applyFormula(const std::vector<double>& src,
                                       const std::string& valueExpr,
                                       const std::string& keepExpr,
                                       const Stats& st,
                                       std::string& err) {
    err.clear();
    std::vector<double> out;
    if (src.empty()) return out;

    std::vector<formula::Var> vars = buildVars(st);
    const std::string vexpr = valueExpr.empty() ? std::string("x") : valueExpr;
    out.reserve(src.size());

    // sorted once here, not per point, otherwise p() would be O(n^2 log n)
    std::vector<double> sorted = src;
    std::sort(sorted.begin(), sorted.end());
    EvalCtx ctx;
    ctx.sorted = &sorted;

    for (size_t k = 0; k < src.size(); k++) {
        vars[0].value = (double)src[k];
        vars[1].value = (double)k;

        if (!keepExpr.empty()) {
            formula::Result keep = formula::eval(keepExpr, vars, percentileFn, &ctx);
            if (!keep.ok()) { err = "keep: " + keep.error; return {}; }
            if (keep.value == 0.0) continue;
        }

        formula::Result v = formula::eval(vexpr, vars, percentileFn, &ctx);
        if (!v.ok()) { err = "value: " + v.error; return {}; }
        out.push_back((double)v.value);
    }
    return out;
}

/// @brief drop `data` into the pool as `name`, overwriting if it's already there
/// overwrite not append, otherwise you'd stack up duplicates every session
inline void publish(const std::string& name, const std::vector<double>& data) {
    if (seriesPool::NamedSeries* existing = seriesPool::findSeries(name)) {
        existing->data.assign(1, data);
        existing->color = seriesPool::resolveColor({});
    } else {
        seriesPool::addLine(name, data);
    }
}

/// @brief rebuild one saved transform's series from its formula
/// we save the recipe, not the numbers, so it re-runs against whatever the
/// strategy loaded this time
/// @return false if it couldn't be rebuilt, e.g. the source is gone
inline bool rebuildPublished(TransformWindow& t) {
    if (t.publishedName[0] == 0) return false;
    if (t.selectedSeriesIdx < 0 || t.selectedSeriesIdx >= (int)seriesPool::pool.size()) return false;

    const seriesPool::NamedSeries& src = seriesPool::pool[t.selectedSeriesIdx];
    if (t.col < 0 || t.col >= src.cols()) return false;

    const std::vector<double> column = src.data[t.col];   // copy, publish may reallocate the pool
    std::string err;
    const std::vector<double> out =
        applyFormula(column, t.valueExpr, t.keepExpr, computeStats(column), err);
    if (!err.empty() || out.empty()) return false;

    publish(t.publishedName, out);
    return true;
}

/// @brief rebuild all of them, for anyone outside the .ini path
inline void restorePublished() {
    for (auto& t : transforms) rebuildPublished(t);
}

/// @brief draw one row of the before/after table
inline void statRow(const char* label, double before, double after, bool isCount) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
    if (isCount) {
        ImGui::TableSetColumnIndex(1); ImGui::Text("%d", (int)before);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%d", (int)after);
        ImGui::TableSetColumnIndex(3);
        const int d = (int)after - (int)before;
        if (d) ImGui::Text("%+d (%.1f%%)", d, before > 0 ? 100.0 * d / before : 0.0);
        else   ImGui::TextDisabled("-");
    } else {
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", before);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", after);
        ImGui::TableSetColumnIndex(3);
        const double d = after - before;
        if (d != 0.0) ImGui::Text("%+.4f", d);
        else          ImGui::TextDisabled("-");
    }
}

/// @brief cheat sheet for what you can type
inline void renderHelp() {
    ImGui::TextDisabled("(?)");
    if (!ImGui::IsItemHovered()) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(460.0);
    ImGui::TextUnformatted(
        "Per point:  x (value)   i (index, from 0)\n"
        "Whole column:  n mean median stdev min max sum p25 p75\n"
        "Percentile:  p(k) for any k from 0 to 100, e.g. p(30) p(95) p(99.5)\n"
        "Constants:  pi e\n"
        "\n"
        "Operators:  + - * / % ^   < <= > >= == !=   && || !   cond ? a : b\n"
        "Functions:  abs sqrt log log10 exp floor ceil round sign\n"
        "            sin cos tan   pow(a,b) min(a,b) max(a,b)\n"
        "            clamp(v,lo,hi)  if(cond,a,b)\n"
        "\n"
        "Value examples:\n"
        "  x - mean            centre on zero\n"
        "  (x - mean) / stdev  z-score\n"
        "  (x - min) / (max - min)   rescale to 0..1\n"
        "  x / max * 100       percent of peak\n"
        "  log(x)              log scale\n"
        "\n"
        "Keep examples (blank keeps everything):\n"
        "  x < p(70)           drop the top 30 percent\n"
        "  x < p75             same as p(75), a shorthand\n"
        "  x > min && x < max  drop the extremes\n"
        "  i % 2 == 0          every other point\n"
        "  abs(x - mean) < 2 * stdev   within 2 sigma");
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

/// @brief render every open transform window for this frame
inline void renderTransforms() {
    // imgui only saves the .ini when something marks it dirty, and it only does
    // that for moves and resizes, so editing a formula has to say so itself
    bool dirty = false;

    for (int ti = 0; ti < (int)transforms.size(); ) {
        TransformWindow& t = transforms[ti];
        const std::string uid = "##tf_" + t.id;
        const std::string title = "Transform " + t.id + "###transform_" + t.id;

        bool open = true;
        ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
        ImGui::Begin(title.c_str(), &open);

        // source series
        const bool haveSel = t.selectedSeriesIdx >= 0
                          && t.selectedSeriesIdx < (int)seriesPool::pool.size();
        const char* preview = haveSel ? seriesPool::pool[t.selectedSeriesIdx].name.c_str()
                                      : "(select series)";
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo(("##src" + uid).c_str(), preview)) {
            for (int i = 0; i < (int)seriesPool::pool.size(); i++) {
                const bool selected = (i == t.selectedSeriesIdx);
                if (ImGui::Selectable(seriesPool::pool[i].name.c_str(), selected)) {
                    t.selectedSeriesIdx = i;
                    t.col = 0;
                    dirty = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (!haveSel) {
            ImGui::TextDisabled("pick a series to transform");
            ImGui::End();
            if (!open) transforms.erase(transforms.begin() + ti); else ++ti;
            continue;
        }

        auto& series = seriesPool::pool[t.selectedSeriesIdx];
        if (series.cols() == 0) {
            ImGui::TextDisabled("(series has no data)");
            ImGui::End();
            if (!open) transforms.erase(transforms.begin() + ti); else ++ti;
            continue;
        }
        if (t.col >= series.cols()) t.col = 0;
        if (series.cols() > 1) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo(("##col" + uid).c_str(), series.colName(t.col).c_str())) {
                for (int c = 0; c < series.cols(); c++)
                    if (ImGui::Selectable(series.colName(c).c_str(), c == t.col)) { t.col = c; dirty = true; }
                ImGui::EndCombo();
            }
        }
        ImGui::SameLine();
        renderHelp();

        const std::vector<double>& src = series.data[t.col];
        const Stats before = computeStats(src);

        // the formulas
        ImGui::SeparatorText("Formula");
        ImGui::TextUnformatted("value");
        ImGui::SameLine(60);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint(("##value" + uid).c_str(), "x",
                                     t.valueExpr, sizeof(t.valueExpr))) dirty = true;
        ImGui::TextUnformatted("keep");
        ImGui::SameLine(60);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint(("##keep" + uid).c_str(), "blank keeps every point",
                                     t.keepExpr, sizeof(t.keepExpr))) dirty = true;

        std::string err;
        const std::vector<double> out = applyFormula(src, t.valueExpr, t.keepExpr, before, err);

        if (!err.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95, 0.45, 0.40, 1.0));
            ImGui::TextWrapped("%s", err.c_str());
            ImGui::PopStyleColor();
        } else if (out.empty()) {
            ImGui::TextDisabled("the keep formula removed every point");
        }

        const Stats after = computeStats(out);

        ImGui::SeparatorText("Stats");
        if (ImGui::BeginTable(("stats" + uid).c_str(), 4,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
              | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("before");
            ImGui::TableSetupColumn("after");
            ImGui::TableSetupColumn("delta");
            ImGui::TableHeadersRow();
            statRow("count",  before.n,      after.n,      true);
            statRow("mean",   before.mean,   after.mean,   false);
            statRow("median", before.median, after.median, false);
            statRow("stdev",  before.stdev,  after.stdev,  false);
            statRow("min",    before.min,    after.min,    false);
            statRow("max",    before.max,    after.max,    false);
            statRow("p25",    before.p25,    after.p25,    false);
            statRow("p75",    before.p75,    after.p75,    false);
            statRow("sum",    before.sum,    after.sum,    false);
            ImGui::EndTable();
        }

        if (ImGui::Checkbox(("show source" + uid).c_str(), &t.showSource)) dirty = true;
        if (ImPlot::BeginPlot(("##plot" + uid).c_str(), ImVec2(-1, 180),
                              ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            if (t.showSource && !src.empty())
                ImPlot::PlotLine("source", src.data(), (int)src.size());
            if (!out.empty())
                ImPlot::PlotLine("result", out.data(), (int)out.size());
            ImPlot::EndPlot();
        }

        // saving makes a separate pool series, source is left alone. the formula
        // is what persists, the numbers get rebuilt next launch
        ImGui::SeparatorText("Save");
        if (t.publishedName[0] != '\0') {
            ImGui::Text("saved as \"%s\", rebuilt on every launch", t.publishedName);
            if (ImGui::Button(("Update now" + uid).c_str()) && !out.empty() && err.empty())
                publish(t.publishedName, out);
            ImGui::SameLine();
            if (ImGui::Button(("Stop saving" + uid).c_str())) {
                t.publishedName[0] = '\0';
                dirty = true;
            }
        } else {
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputTextWithHint(("##name" + uid).c_str(), "new series name",
                                         t.outName, sizeof(t.outName))) dirty = true;
            ImGui::SameLine();
            const bool named = t.outName[0] != '\0';
            ImGui::BeginDisabled(!named || out.empty() || !err.empty());
            if (ImGui::Button(("Save to pool" + uid).c_str())) {
                publish(std::string(t.outName), out);
                for (size_t k = 0; k < sizeof(t.publishedName); k++)
                    t.publishedName[k] = k < sizeof(t.outName) ? t.outName[k] : '\0';
                t.publishedName[sizeof(t.publishedName) - 1] = '\0';
                dirty = true;
            }
            ImGui::EndDisabled();
            if (!named && err.empty() && !out.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(name it first)");
            }
        }

        ImGui::End();
        if (!open) { transforms.erase(transforms.begin() + ti); dirty = true; }
        else ++ti;
    }

    if (dirty) ImGui::MarkIniSettingsDirty();
}

inline void* iniReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    newTransform(name);
    return (void*)findTransform(name);
}

/// @brief one key per line, so a formula with '|' or ',' in it survives
inline void iniReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    if (!entry) return;
    TransformWindow& t = *(TransformWindow*)entry;
    const std::string ln(line);
    const size_t eq = ln.find('=');
    if (eq == std::string::npos) return;
    const std::string key = ln.substr(0, eq);
    const std::string val = ln.substr(eq + 1);

    auto copyInto = [](char* dst, size_t cap, const std::string& v) {
        const size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
        for (size_t i = 0; i < n; i++) dst[i] = v[i];
        dst[n] = '\0';
    };

    if      (key == "Series") {
        for (int i = 0; i < (int)seriesPool::pool.size(); i++)
            if (seriesPool::pool[i].name == val) { t.selectedSeriesIdx = i; break; }
    }
    else if (key == "Col")    t.col = std::atoi(val.c_str());
    else if (key == "Value")  copyInto(t.valueExpr, sizeof(t.valueExpr), val);
    else if (key == "Keep")   copyInto(t.keepExpr,  sizeof(t.keepExpr),  val);
    else if (key == "Source") t.showSource = val != "0";
    else if (key == "Saved") {
        copyInto(t.publishedName, sizeof(t.publishedName), val);
        // Saved comes last in the file so everything else is already set, and
        // panels look their series up while reading, so it has to exist by then
        rebuildPublished(t);
    }
}

inline void iniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    for (auto& t : transforms) {
        buf->appendf("[%s][%s]\n", handler->TypeName, t.id.c_str());
        const std::string name = (t.selectedSeriesIdx >= 0
                               && t.selectedSeriesIdx < (int)seriesPool::pool.size())
            ? seriesPool::pool[t.selectedSeriesIdx].name : "";
        buf->appendf("Series=%s\n", name.c_str());
        buf->appendf("Col=%d\n", t.col);
        buf->appendf("Value=%s\n", t.valueExpr);
        buf->appendf("Keep=%s\n", t.keepExpr);
        buf->appendf("Source=%d\n", t.showSource ? 1 : 0);
        // keep Saved last, the reader rebuilds off this key
        buf->appendf("Saved=%s\n", t.publishedName);
        buf->appendf("\n");
    }
}

/// @brief hook into imgui's .ini so these come back on startup
/// call once, before the first NewFrame()
inline void registerSettingsHandler() {
    ImGuiSettingsHandler h;
    h.TypeName   = "AZTransform";
    h.TypeHash   = ImHashStr("AZTransform");
    h.ReadOpenFn = iniReadOpen;
    h.ReadLineFn = iniReadLine;
    h.WriteAllFn = iniWriteAll;
    ImGui::AddSettingsHandler(&h);
}

} // namespace transformManagement
