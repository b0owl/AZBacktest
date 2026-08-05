// check if actual equity and MC paths have same length
#include <iostream>
#include <vector>
#include "src/backtestApi.h"

int main() {
    float tickSize = 0.25f, tickValue = 0.50f;
    MarketData md(kCSVMapping.path);
    std::vector<float> prices;
    Handling h(prices, tickSize, tickValue);
    const int rthOpen = 13*3600+30*60, rthClose = 20*3600, warmupSecs = 900;
    std::vector<float> dailyPrices, dailyVolume;
    int currentDay = -1;
    float val = 0.f, vah = 0.f;
    int barsSinceProfile = 0, i = 0;

    while (true) {
        auto window = h.requestDataWindow(md, 500, 30);
        if (window.prices.empty()) break;
        prices = std::move(window.prices);
        auto& volumes = window.volumes;
        for (int b = 0; b < (int)prices.size(); b++) {
            i++;
            float px = prices[b];
            long long epochSec = h.windowTimestamps[b];
            int tod = (int)(epochSec % 86400);
            if (tod < rthOpen || tod >= rthClose) {
                if (h.openTrade) {
                    float saved = prices.back(); prices.back() = px;
                    h.lastEpochSec = epochSec; h.tick(); h.closeTrade();
                    prices.back() = saved;
                }
                continue;
            }
            int day = (int)(epochSec / 86400);
            if (day != currentDay) {
                currentDay = day; dailyPrices.clear(); dailyVolume.clear();
                val = vah = 0.f; barsSinceProfile = 0;
            }
            dailyPrices.push_back(px); dailyVolume.push_back(volumes[b]);
            if (tod - rthOpen < warmupSecs) continue;
            barsSinceProfile++;
            if (barsSinceProfile >= 50 || val == 0.f) {
                auto profile = returnVolumeProfile(0, dailyPrices, dailyVolume);
                if (!profile.empty()) { auto va = returnValueArea(profile); val = va[0]; vah = va[1]; }
                barsSinceProfile = 0;
            }
            if (val == 0.f && vah == 0.f) continue;
            float saved = prices.back(); prices.back() = px;
            h.lastEpochSec = epochSec; h.tick();
            if (h.openTrade) { float pnl = h.openTrade->td.profit; if (pnl >= 100.f || pnl <= -50.f) h.closeTrade(); }
            if (!h.inShort && px >= vah) h.openShort(i);
            if (!h.inLong && px <= val) h.openLong(i);
            float mid = (val + vah) / 2.f;
            if (h.inShort && px <= mid) h.closeTrade();
            if (h.inLong && px >= mid) h.closeTrade();
            prices.back() = saved;
        }
    } h.closeAll();

    auto profit = returnCumProfitBucketed(86400);
    auto mcPaths = returnMonteCarlo(5, 5, 86400);
    auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});

    std::cout << "trades: " << trades.size() << "\n";
    std::cout << "actual equity length: " << profit.size() << "\n";
    std::cout << "mc path length:       " << mcPaths[0].size() << "\n";
    std::cout << "pct path length:      " << pctPaths[0].size() << "\n";

    // print first/last few values of actual vs mc[0]
    std::cout << "\nactual first 5: ";
    for (int j = 0; j < 5 && j < (int)profit.size(); j++) std::cout << profit[j] << " ";
    std::cout << "\nmc[0]  first 5: ";
    for (int j = 0; j < 5 && j < (int)mcPaths[0].size(); j++) std::cout << mcPaths[0][j] << " ";
    std::cout << "\nactual last 5:  ";
    for (int j = std::max(0,(int)profit.size()-5); j < (int)profit.size(); j++) std::cout << profit[j] << " ";
    std::cout << "\nmc[0]  last 5:  ";
    for (int j = std::max(0,(int)mcPaths[0].size()-5); j < (int)mcPaths[0].size(); j++) std::cout << mcPaths[0][j] << " ";
    std::cout << "\n";
}
