#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"
#include "../src/skins/toxic.h"

int main() {
    loadConfig();

    // Handling reads prices.back() as "the price right now", so this vector only
    // ever holds the bar being processed
    std::vector<float> prices;
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25f, 0.50f);

    const int timeframe = 60;   // seconds per bar
    const int batchSize = 500;  // rows per read, an io detail, not a strategy knob

    handler.fetchEOF(timeframe);

    int bar = 0;
    for (;;) {
        DataWindow window = handler.requestDataWindow(md, batchSize, timeframe);
        if (window.prices.empty()) break;

        for (std::size_t b = 0; b < window.prices.size(); b++, bar++) {
            if (bar % 5000 == 0)
                std::cout << "  bar " << bar << " / " << handler.eof << std::endl;

            prices.assign(1, window.prices[b]);

            // mark the open trade to this bar and stamp the equity curve. the
            // timestamp matters, without it trades close at epoch 0 and anything
            // time bucketed downstream collapses into one bucket
            handler.tick(handler.windowTimestamps[b]);

            // buy the first bar, then just sit in it until closeAll below
            if (!handler.inLong) handler.openLong(bar);
        }
    }
    handler.closeAll();

    auto profit = returnProfitOverTime(1440);
    addLine("equity", std::vector<std::vector<float>>{profit}, {"actual"},
            RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    showConsole("Console", skins::dark);
}
