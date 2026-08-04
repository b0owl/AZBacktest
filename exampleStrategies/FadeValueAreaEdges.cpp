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

int main() {
    float tickSize  = 0.25f;
    float tickValue = 0.50f;
    MarketData md(kCSVMapping.path);

    std::vector<float> prices;

    Handling h(prices, tickSize, tickValue);

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
        prices = h.requestDataWindow(md, batchSize, 30);
        if (prices.empty()) break;

        for (int b = 0; b < (int)prices.size(); b++) {
            i++;
            float px = prices[b];
            long long epochSec = h.windowTimestamps[b];

            // skip bars that span data gaps, not real 30s bars
            if (prevEpoch > 0 && (epochSec - prevEpoch) > 120) {
                if (h.openTrade) {
                    // close at the last valid price, not the post-gap price
                    h.closeTrade();
                }
                prevEpoch = epochSec;
                continue;
            }
            prevEpoch = epochSec;

            int tod = (int)(epochSec % 86400);

            if (tod < rthOpen || tod >= rthClose) {
                // close any open trade at RTH boundary so overnight gaps dont inflate PnL
                if (h.openTrade) {
                    float saved = prices.back();
                    prices.back() = px;
                    h.lastEpochSec = epochSec;
                    h.tick();
                    h.closeTrade();
                    prices.back() = saved;
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
            dailyVolume.push_back(px);

            if (i % 5000 == 0) { std::cout << "  bar " << i << " trades=" << trades.size() << "\n"; }

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

            // temporarily set prices.back() so Handling reads the right price
            float saved = prices.back();
            prices.back() = px;
            h.lastEpochSec = epochSec;
            h.tick();

            if (h.openTrade) {
                float pnl = h.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) h.closeTrade();
            }

            float mid = (val + vah) / 2.f;
            if (!h.inShort && px >= vah) { h.openShort(i); targetMid = mid; }
            if (!h.inLong  && px <= val) { h.openLong(i);  targetMid = mid; }

            if (h.inShort && px <= targetMid) h.closeTrade();
            if (h.inLong  && px >= targetMid) h.closeTrade();

            prices.back() = saved;
        }
    } h.closeAll();

    std::cout << "bars processed: " << i << "\n";
    std::cout << "trades:         " << trades.size() << "\n";
    std::cout << "winrate:        " << returnWinrate() * 100.0f << "%\n";
    std::cout << "cum profit:     " << returnCumProfit() << " pts\n";

    // stats
    addStat("winrate %", returnWinrate() * 100.f);
    addStat("expectancy", returnAvgPnl());
    addStat("avg winner", returnAverageWinSize());
    addStat("avg loser", returnAverageLossSize());
    addStat("trades/day", returnTradesPerDay());
    addStat("total trades", (float)trades.size());
    addStat("cum profit", returnCumProfit());

    // monte carlo (daily bucketed)
    int mcSims = 60;
    auto mcPaths = returnMonteCarlo(mcSims, 5, 86400);
    auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});
    auto profit = returnCumProfitBucketed(86400);

    std::vector<std::vector<float>> mainPaths;
    mainPaths.push_back(profit);
    for (auto& p : pctPaths) mainPaths.push_back(std::move(p));

    addSeries("mc cloud", mcPaths, {}, 0, RGBA{0.4f, 0.4f, 0.4f, 0.08f});
    addSeries("equity + percentiles", mainPaths,
        {"actual", "p5", "p50", "p95"}, 0, RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    showConsole("Console");
}
