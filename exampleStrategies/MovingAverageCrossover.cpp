#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"
#include "../src/skins/toxic.h"
#include "../src/skins/gilded.h"

int main() {
    loadConfig();

    // Handling reads prices.back() as "the price right now", so this vector only
    // ever holds the bar being processed. indicator history is kept separately
    std::vector<double> prices;
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25, 0.50);

    const int   shortPeriod = 100;
    const int   longPeriod  = 200;
    const double takeProfit  = 500.0;
    const double stopLoss    = 50.0;

    const int timeframe = 60;   // seconds per bar
    const int batchSize = 500;  // rows per read, an io detail, not a strategy knob

    handler.fetchEOF(timeframe);

    // trailing window of closes, capped at longPeriod so the work per bar is flat
    // rather than growing with the length of the backtest
    std::vector<double> history;
    history.reserve(longPeriod);

    int bar = 0;
    for (;;) {
        DataWindow window = handler.requestDataWindow(md, batchSize, timeframe);
        if (window.prices.empty()) break;

        for (std::size_t b = 0; b < window.prices.size(); b++, bar++) {
            if (bar % 5000 == 0)
                std::cout << "  bar " << bar << " / " << handler.eof << std::endl;

            // this bar is now the current price, and joins the trailing history
            prices.assign(1, window.prices[b]);
            history.push_back(window.prices[b]);
            if ((int)history.size() > longPeriod) history.erase(history.begin());

            // mark the open trade to this bar and stamp the equity curve. the
            // timestamp matters, without it trades close at epoch 0 and anything
            // time bucketed downstream collapses into one bucket
            handler.tick(handler.windowTimestamps[b]);

            // risk first, so a runner gets cut before any signal work
            if (handler.openTrade) {
                const double pnl = handler.openTrade->td.profit;
                if (pnl >= takeProfit || pnl <= -stopLoss) handler.closeTrade();
            }

            if ((int)history.size() < longPeriod) continue; // not enough history yet

            // recomputed every bar from closes up to and including this one, so a
            // signal can never be built out of prices that haven't happened
            PriceAnalytics pa(history);
            const double shortMa = pa.returnSimpleMovingAverage(shortPeriod).back();
            const double longMa  = pa.returnSimpleMovingAverage(longPeriod).back();

            const bool shortAbove = shortMa > longMa;
            const bool shortBelow = shortMa < longMa;

            // exits first so we can flip straight into the opposite side
            if (handler.inLong  && shortBelow) handler.closeTrade();
            if (handler.inShort && shortAbove) handler.closeTrade();

            if (!handler.inLong  && shortAbove) handler.openLong(bar);
            if (!handler.inShort && shortBelow) handler.openShort(bar);
        }
    }
    handler.closeAll();

    // monte carlo (daily bucketed)
    const int mcSims = 60;
    auto mcPaths  = returnMonteCarlo(mcSims, 5, 86400);
    auto pctPaths = returnPercentilePaths(mcPaths, {5, 50, 95});
    auto profit   = returnCumProfitBucketed(86400);

    std::vector<std::vector<double>> mainPaths;
    mainPaths.push_back(profit);
    for (auto& p : pctPaths) mainPaths.push_back(std::move(p));

    addLine("mc cloud", mcPaths, {}, RGBA{0.4f, 0.4f, 0.4f, 0.3f});
    addLine("equity + percentiles", mainPaths,
        {"actual", "p5", "p50", "p95"}, RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    showConsole("Console", skins::toxic);
}
