#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"
#include "../src/skins/toxic.h"

int main() {
    // prices has to outlive handler, which holds a reference to it
    loadConfig();
    std::vector<float> prices;
    MarketData md(kCSVMapping.path);
    Handling handler(prices, 0.25f, 0.50f);

    handler.fetchEOF(60);
    int batchSize = 500;
    for (int i = 0; ++i;) {
        auto window = handler.requestDataWindow(md, batchSize, 60);
        if (window.prices.empty()) break;
        prices = std::move(window.prices);

        for (int b = 0; b < (int)prices.size(); b++) {
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << handler.eof << std::endl; }

            handler.openLong(i);

            float saved = prices.back();
            prices.back() = prices[b];
            handler.tick(handler.windowTimestamps[b]);
            prices.back() = saved;
        }
    }
    handler.closeAll();

    auto profit = returnProfitOverTime(1440);
    addSeries("equity", std::vector<std::vector<float>>{profit}, {"actual"}, "line", RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    showConsole("Console", skins::dark);
}
