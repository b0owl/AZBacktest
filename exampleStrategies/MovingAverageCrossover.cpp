// TODO - Clean up other files


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

    int shortPeriod = 100;
    int longPeriod  = 200;

    float takeProfit = 500.f;
    float stopLoss   = 50.f;

    int i = 0;
    while (auto tick = md.nextTick()) {
        i++;

        if (i % 500 == 0) { // loads 500 rows, 60s tf
            auto window = h.requestDataWindow(md, 500, 60);
            prices = std::move(window.prices);
        }
        if (i % 1000 == 0) { std::cout << "  bar " << i << " trades=" << trades.size() << "\n"; } // counter

        // Check window size
        if ((int)prices.size() < (longPeriod * 2)) continue; // long period * 2 = min window

        float shortMaVal = returnSimpleMovingAverage(prices, shortPeriod).back();
        float longMaVal  = returnSimpleMovingAverage(prices, longPeriod).back();

        bool shortAboveLong = shortMaVal > longMaVal;
        bool shortBelowLong = shortMaVal < longMaVal;

        // This can return state directtly; or you could access it via h.inLong/h.inShort if you don't care about
        // ownership. (You could always assign ownership by setting a local inX var to h.inX but I digress)

        // Tick is needed for things like letting the handler know the state of a trade
        // be sure to call it inside of your loop
        h.tick(tick->timestamp);

        // tp/sl handling
        if (h.openTrade) {
            float pnl = h.openTrade->td.profit;
            if (pnl >= takeProfit || pnl <= -stopLoss) h.closeTrade();
        }

        // Exits first so we can immediately flip into the opposite side on the same bar
        if (h.inLong && shortBelowLong)  h.closeTrade();
        if (h.inShort && shortAboveLong) h.closeTrade();

        // Entries
        if (!h.inLong && shortAboveLong)  h.openLong(i);
        if (!h.inShort && shortBelowLong) h.openShort(i);
    } h.closeAll();

    std::cout << "bars processed: " << i << "\n";
    std::cout << "trades:         " << trades.size() << "\n";
    std::cout << "winrate:        " << returnWinrate() * 100.0f << "%\n";
    std::cout << "cum profit:     " << returnCumProfit() << " pts\n";

    auto profit = returnProfitOverTime();
    initSeriesPool(
        {profit, profit, returnAverageProfitOverTime()},
        {"profit over time (line)", "profit over time (bar)", "average profit over time"},
        {0, 1, 0}
    );
    showConsole("Console");
}
