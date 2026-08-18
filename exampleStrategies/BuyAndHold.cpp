#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    auto engine = setupEngine(0.25f, 0.50f);

    engine.handler.fetchEOF(60);
    int batchSize = 500;
    for (int i = 0; ++i;) {
        auto window = engine.handler.requestDataWindow(engine.md, batchSize, 60);
        if (window.prices.empty()) break;
        engine.prices = std::move(window.prices);

        for (int b = 0; b < (int)engine.prices.size(); b++) {
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << engine.handler.eof << std::endl; }

            engine.handler.openLong(i);

            float saved = engine.prices.back();
            engine.prices.back() = engine.prices[b];
            engine.handler.tick(engine.handler.windowTimestamps[b]);
            engine.prices.back() = saved;
        }
    }
    engine.handler.closeAll();

    auto profit = returnProfitOverTime(1440);
    addSeries("equity", std::vector<std::vector<float>>{profit}, {"actual"}, 0, RGBA{0.5f, 0.8f, 0.5f, 1.0f});

    std::cout << "Thanks for trying out my project :)\n"
              << "You can change the theme by calling showConsole with the appropriate function type "
                 "(see the bottom of any example file); all normal ImGUI attributes are customizeable.\n";

    showConsole("Console", skins::dark);
    // showConsole("Console", skins::light);
}
