#pragma once
#include <string>
#include <vector>
#include <type_traits>

namespace seriesPool {

/// @brief rgba color, all -1 means "let the renderer pick"
struct RGBA {
    float r=-1, g=-1, b=-1, a=-1;
    bool isSet() const { return r >= 0; }
};

/// @brief one entry in the global series pool
/// data is column-major: data[col][row], a simple 1D series has one column
struct NamedSeries {
    std::string name;
    std::vector<std::vector<float>> data;      ///< data[column][row]
    std::vector<std::string> columnNames;       ///< optional per-column names
    int type;                                   ///< 0 = lineplot, 1 = barplot
    RGBA color;                                 ///< all columns share this color, -1 = auto
    bool onY2 = false;                          ///< default axis when added to a panel, right (Y2) vs left (Y1)
    bool xyBars = false;                        ///< true = data[0] is x, data[1] is y (e.g. a histogram) instead of index-based x
    float barWidth = 0.67f;                     ///< only used when xyBars is set
    int heatmapRows = 0;                        ///< heatmap row count (type 2 only)
    int heatmapCols = 0;                        ///< heatmap col count (type 2 only)

    int cols() const { return (int)data.size(); }       ///< number of columns (1 for a simple series)
    int rows() const { return data.empty() ? 0 : (int)data[0].size(); } ///< number of data points per column

    /// @brief get display name for a column, falls back to index if unnamed
    std::string colName(int col) const {
        if (col < (int)columnNames.size() && !columnNames[col].empty())
            return columnNames[col];
        return std::to_string(col);
    }
};

/// @brief process-wide pool of series, everything that's been added lives here
inline std::vector<NamedSeries> pool;

/// @brief wipe the pool so you can start fresh (e.g. between backtests)
inline void clear() { pool.clear(); }

/// @brief find a series by name, or nullptr if it doesn't exist
inline NamedSeries* findSeries(const std::string& name) {
    for (auto& s : pool) if (s.name == name) return &s;
    return nullptr;
}

/// @brief add a 1D series (single column) to the pool
/// @param name  display name for the series
/// @param values the data, any arithmetic type gets converted to float
/// @param type  0 = line, 1 = bar
/// @param onY2  default axis when this series gets added to a panel, true = right (Y2)
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void addSeries(std::string name, std::vector<T> values, int type = 0, RGBA color = {}, bool onY2 = false) {
    std::vector<float> col(values.begin(), values.end());
    pool.push_back({std::move(name), {std::move(col)}, {}, type, color, onY2});
}

/// @brief add a 2D series (multiple columns) to the pool
/// @param name     display name for the series
/// @param values   vector of columns, each column is a vector of values
/// @param colNames optional names for each column (shows up in explorer + plot legend)
/// @param type     0 = line, 1 = bar
/// @param onY2     default axis when this series gets added to a panel, true = right (Y2)
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void addSeries(std::string name, std::vector<std::vector<T>> values,
               std::vector<std::string> colNames = {}, int type = 0, RGBA color = {}, bool onY2 = false) {
    std::vector<std::vector<float>> cols;
    cols.reserve(values.size());
    for (auto& v : values) {
        cols.emplace_back(v.begin(), v.end());
    }
    pool.push_back({std::move(name), std::move(cols), std::move(colNames), type, color, onY2});
}

/// @brief add an (x,y) bar series, e.g. a histogram: xs=bucket value, ys=occurrences
/// @param name     display name for the series
/// @param xs       x-coordinate per bar (e.g. bucket edges)
/// @param ys       bar height per x (e.g. counts), same length as xs
/// @param barWidth width of each bar in x-axis units
template<typename Tx, typename Ty, typename = std::enable_if_t<std::is_arithmetic_v<Tx> && std::is_arithmetic_v<Ty>>>
void addXYBars(std::string name, std::vector<Tx> xs, std::vector<Ty> ys, float barWidth = 0.67f,
               RGBA color = {}, bool onY2 = false) {
    std::vector<float> xf(xs.begin(), xs.end());
    std::vector<float> yf(ys.begin(), ys.end());
    NamedSeries s{std::move(name), {std::move(xf), std::move(yf)}, {}, 1, color, onY2};
    s.xyBars = true;
    s.barWidth = barWidth;
    pool.push_back(std::move(s));
}

/// @brief add a heatmap series to the pool
/// @param name   display name
/// @param values flat row-major data (rows * cols elements)
/// @param rows   number of rows
/// @param cols   number of columns
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void addHeatmap(std::string name, std::vector<T> values, int rows, int cols, RGBA color = {}) {
    std::vector<float> flat(values.begin(), values.end());
    NamedSeries s{std::move(name), {std::move(flat)}, {}, 2, color, false};
    s.heatmapRows = rows;
    s.heatmapCols = cols;
    pool.push_back(std::move(s));
}

/// @brief batch init for the simple case where each inner vector is its own
/// 1-column series. clears the pool first so this is a full reset
/// @param data  one series per outer vector
/// @param names display name per series (auto-named if shorter than data)
/// @param types 0 = line, 1 = bar per series (defaults to 0 if shorter)
inline void initSeriesPool(std::vector<std::vector<float>> data,
                           std::vector<std::string> names,
                           std::vector<int> types) {
    pool.clear();
    pool.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        std::string n = i < names.size() ? names[i] : "series " + std::to_string(i);
        int t = i < types.size() ? types[i] : 0;
        pool.push_back({std::move(n), {std::move(data[i])}, {}, t, {}});
    }
}

} // namespace seriesPool
