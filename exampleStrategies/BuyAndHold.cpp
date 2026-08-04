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

    int i = 0;
    while (auto tick = md.nextTick()) {
        i++;

        if (i % 500 == 0) {
            prices = h.requestDataWindow(md, 500, 60); // loads 500 rows, 60s tf
        }

        if (i % 1000 == 0) { std::cout << "  bar " << i << " trades=" << trades.size() << "\n"; } // counter

        if ((int)prices.size() < 1) continue; 

        h.openLong(i);

        // Advance the open trade so its P&L reflects the current bar before exit checks
        h.tick(tick->timestamp);
    } h.closeAll(); // closes the trade

    std::cout << "bars processed: " << i << "\n";
    std::cout << "trades:         " << trades.size() << "\n";
    std::cout << "winrate:        " << returnWinrate() * 100.0f << "%\n";
    std::cout << "cum profit:     " << returnCumProfit() << " pts\n";

    auto profit = returnProfitOverTime(60 * 24); // one bucket per day
    initSeriesPool(
        {profit, profit, returnAverageProfitOverTime()},
        {"profit over time (line)", "profit over time (bar)", "average profit over time"},
        {0, 1, 0}
    );
    showConsole("Console");
}
