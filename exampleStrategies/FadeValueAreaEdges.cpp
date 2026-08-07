// fade the edges of the value area
// short when price touches VAH, long when price touches VAL
// idea is that price tends to revert back into the value area

// this is the only semi-serious algo in the examples, so it will also be more fitted out with
// features (i.e mc paths, etc)

// builds a daily volume profile from the current day's RTH bars
// waits 15 minutes into RTH before trading


#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    auto e = setupEngine(0.25f, 0.50f);

    float takeProfit = 100.f;
    float stopLoss   = 50.f;

    // RTH window in seconds-since-midnight UTC
    const int rthOpen  = 13*3600 + 30*60;
    const int rthClose = 20*3600;
    const int warmupSecs = 900;

    // daily state
    std::vector<float> dailyPrices;
    std::vector<float> dailyVolume;
    int currentDay = -1;
    float val = 0.f, vah = 0.f;
    float targetMid = 0.f; // snapshot mid at entry so shifting VA doesnt inflate PnL
    int barsSinceProfile = 0;

    int batchSize = 500;
    int i = 0;
    long long prevEpoch = 0;
    while (true) {
        auto window = e.h.requestDataWindow(e.md, batchSize, 30);
        if (window.prices.empty()) break;
        e.prices = std::move(window.prices);
        auto& volumes = window.volumes;

        for (int b = 0; b < (int)e.prices.size(); b++) {
            i++;
            float px = e.prices[b];
            long long epochSec = e.h.windowTimestamps[b];

            // skip bars that span data gaps, not real 30s bars
            if (prevEpoch > 0 && (epochSec - prevEpoch) > 120) {
                if (e.h.openTrade) {
                    e.h.closeTrade();
                }
                prevEpoch = epochSec;
                continue;
            }
            prevEpoch = epochSec;

            int tod = (int)(epochSec % 86400);

            if (tod < rthOpen || tod >= rthClose) {
                if (e.h.openTrade) {
                    float saved = e.prices.back();
                    e.prices.back() = px;
                    e.h.lastEpochSec = epochSec;
                    e.h.tick();
                    e.h.closeTrade();
                    e.prices.back() = saved;
                }
                continue;
            }

            int day = (int)(epochSec / 86400);
            if (day != currentDay) {
                currentDay = day;
                dailyPrices.clear();
                dailyVolume.clear();
                val = vah = 0.f;
                barsSinceProfile = 0;
            }

            dailyPrices.push_back(px);
            dailyVolume.push_back(volumes[b]);

            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << e.h.eof << std::endl; }

            if (tod - rthOpen < warmupSecs) continue;

            barsSinceProfile++;
            if (barsSinceProfile >= 50 || val == 0.f) {
                auto profile = returnVolumeProfile(0, dailyPrices, dailyVolume);
                if (!profile.empty()) {
                    auto va = returnValueArea(profile);
                    val = va[0]; vah = va[1];
                }
                barsSinceProfile = 0;
            }

            if (val == 0.f && vah == 0.f) continue;

            float saved = e.prices.back();
            e.prices.back() = px;
            e.h.lastEpochSec = epochSec;
            e.h.tick();

            if (e.h.openTrade) {
                float pnl = e.h.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) e.h.closeTrade();
            }

            float mid = (val + vah) / 2.f;
            if (!e.h.inShort && px >= vah) { e.h.openShort(i); targetMid = mid; }
            if (!e.h.inLong  && px <= val) { e.h.openLong(i);  targetMid = mid; }

            if (e.h.inShort && px <= targetMid) e.h.closeTrade();
            if (e.h.inLong  && px >= targetMid) e.h.closeTrade();

            e.prices.back() = saved;
        }
    } e.h.closeAll();

    // monte carlo (daily bucketed)
    int mcSims = 60;
    auto mcPaths = returnMonteCarlo(mcSims, 5, 86400);
    auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});
    auto profit = returnCumProfitBucketed(86400);

    std::vector<std::vector<float>> mainPaths;
    mainPaths.push_back(profit);
    for (auto& p : pctPaths) mainPaths.push_back(std::move(p));

    addSeries("mc cloud", mcPaths, {}, 0, RGBA{0.4f, 0.4f, 0.4f, 0.3f});
    addSeries("equity + percentiles", mainPaths,
        {"actual", "p5", "p50", "p95"}, 0, RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    std::cout << "Thanks for trying out my project :)\n"
              << "You can change the theme by calling showConsole with the appropriate function type "
                 "(see the bottom of any example file); all normal ImGUI attributes are customizeable.\n";

    showConsole("Console", skins::light);
    // showConsole("Console", skins::dark);
}
