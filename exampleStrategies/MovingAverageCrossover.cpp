#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    // prices has to outlive handler, which holds a reference to it
    loadConfig();
    std::vector<float> prices;
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25f, 0.50f);

    int shortPeriod = 100;
    int longPeriod  = 200;

    float takeProfit = 500.f;
    float stopLoss   = 50.f;

    std::cout << "Fetching EOF..." << std::endl;
    handler.fetchEOF(60);
    std::cout << "EOF Found, continuing..." << std::endl;

    int batchSize = 500;
    int i = 0;
    while (true) {
        auto window = handler.requestDataWindow(md, batchSize, 60);
        if (window.prices.empty()) break;
        prices = std::move(window.prices);

        if ((int)prices.size() < (longPeriod * 2)) continue;

        float shortMaVal = returnSimpleMovingAverage(prices, shortPeriod).back();
        float longMaVal  = returnSimpleMovingAverage(prices, longPeriod).back();

        bool shortAboveLong = shortMaVal > longMaVal;
        bool shortBelowLong = shortMaVal < longMaVal;

        for (int b = 0; b < (int)prices.size(); b++) {
            i++;
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << handler.eof << std::endl; }

            float saved = prices.back();
            prices.back() = prices[b];
            handler.tick();

            // tp/sl handling
            if (handler.openTrade) {
                float pnl = handler.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) handler.closeTrade();
            }

            // exits first so we can immediately flip into the opposite side on the same bar
            if (handler.inLong && shortBelowLong)  handler.closeTrade();
            if (handler.inShort && shortAboveLong) handler.closeTrade();

            // entries
            if (!handler.inLong && shortAboveLong)  handler.openLong(i);
            if (!handler.inShort && shortBelowLong) handler.openShort(i);

            prices.back() = saved;
        }
    } handler.closeAll();

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
