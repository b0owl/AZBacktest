#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    auto engine = setupEngine(0.25f, 0.50f);

    int shortPeriod = 100;
    int longPeriod  = 200;

    float takeProfit = 500.f;
    float stopLoss   = 50.f;

    engine.handler.fetchEOF(60);

    int batchSize = 500;
    for (int i = 0; ++i;) {
        auto window = engine.handler.requestDataWindow(engine.md, batchSize, 60);
        if (window.prices.empty()) break;
        engine.prices = std::move(window.prices);

        if ((int)engine.prices.size() < (longPeriod * 2)) continue;

        PriceAnalytics pa(engine.prices);
        float shortMaVal = pa.returnSimpleMovingAverage(shortPeriod).back();
        float longMaVal  = pa.returnSimpleMovingAverage(longPeriod).back();

        bool shortAboveLong = shortMaVal > longMaVal;
        bool shortBelowLong = shortMaVal < longMaVal;

        for (int b = 0; b < (int)engine.prices.size(); b++) {
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << engine.handler.eof << std::endl; }

            float saved = engine.prices.back();
            engine.prices.back() = engine.prices[b];
            engine.handler.tick();

            // tp/sl
            if (engine.handler.openTrade) {
                float pnl = engine.handler.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) engine.handler.closeTrade();
            }

            // exits first so we can immediately flip into the opposite side
            if (engine.handler.inLong && shortBelowLong)  engine.handler.closeTrade();
            if (engine.handler.inShort && shortAboveLong) engine.handler.closeTrade();

            // entries
            if (!engine.handler.inLong && shortAboveLong)  engine.handler.openLong(i);
            if (!engine.handler.inShort && shortBelowLong) engine.handler.openShort(i);

            engine.prices.back() = saved;
        }
    }
    engine.handler.closeAll();

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
