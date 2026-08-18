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
    auto engine = setupEngine(0.25f, 0.50f);

    float takeProfit = 100.f;
    float stopLoss   = 50.f;

    // RTH window in seconds-since-midnight UTC
    const int rthOpen  = 13*3600 + 30*60;
    const int rthClose = 20*3600;
    const int warmupSecs = 900;

    // daily state
    PriceAnalytics pa;
    int currentDay = -1;
    float val = 0.f, vah = 0.f;
    float targetMid = 0.f; // snapshot mid at entry so shifting VA doesnt inflate PnL
    int barsSinceProfile = 0;

    int batchSize = 500;
    int i = 0;
    long long prevEpoch = 0;
    while (true) {
        auto window = engine.handler.requestDataWindow(engine.md, batchSize, 30);
        if (window.prices.empty()) break;
        engine.prices = std::move(window.prices);
        auto& volumes = window.volumes;

        for (int b = 0; b < (int)engine.prices.size(); b++) {
            i++;
            float px = engine.prices[b];
            long long epochSec = engine.handler.windowTimestamps[b];

            // skip bars that span data gaps, not real 30s bars
            if (prevEpoch > 0 && (epochSec - prevEpoch) > 120) {
                if (engine.handler.openTrade) {
                    engine.handler.closeTrade();
                }
                prevEpoch = epochSec;
                continue;
            }
            prevEpoch = epochSec;

            int tod = (int)(epochSec % 86400);

            if (tod < rthOpen || tod >= rthClose) {
                if (engine.handler.openTrade) {
                    float saved = engine.prices.back();
                    engine.prices.back() = px;
                    engine.handler.lastEpochSec = epochSec;
                    engine.handler.tick();
                    engine.handler.closeTrade();
                    engine.prices.back() = saved;
                }
                continue;
            }

            int day = (int)(epochSec / 86400);
            if (day != currentDay) {
                currentDay = day;
                pa = PriceAnalytics();
                val = vah = 0.f;
                barsSinceProfile = 0;
            }

            pa.updatePrices(px, volumes[b]);

            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << engine.handler.eof << std::endl; }

            if (tod - rthOpen < warmupSecs) continue;

            barsSinceProfile++;
            if (barsSinceProfile >= 50 || val == 0.f) {
                auto profile = pa.returnVolumeProfile(0);
                if (!profile.empty()) {
                    auto va = pa.returnValueArea(profile);
                    val = va[0]; vah = va[1];
                }
                barsSinceProfile = 0;
            }

            if (val == 0.f && vah == 0.f) continue;

            float saved = engine.prices.back();
            engine.prices.back() = px;
            engine.handler.lastEpochSec = epochSec;
            engine.handler.tick();

            if (engine.handler.openTrade) {
                float pnl = engine.handler.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) engine.handler.closeTrade();
            }

            float mid = (val + vah) / 2.f;
            if (!engine.handler.inShort && px >= vah) { engine.handler.openShort(i); targetMid = mid; }
            if (!engine.handler.inLong  && px <= val) { engine.handler.openLong(i);  targetMid = mid; }

            if (engine.handler.inShort && px <= targetMid) engine.handler.closeTrade();
            if (engine.handler.inLong  && px >= targetMid) engine.handler.closeTrade();

            engine.prices.back() = saved;
        }
    } engine.handler.closeAll();

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
