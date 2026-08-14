#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    // removes some boilerplate code, not necessarily needed but cleaner to use
    auto engine = setupEngine(0.25, 0.50); // tickSize & tickValue

    std::cout << "Fetching EOF..." << std::endl;
    engine.handler.fetchEOF(60); // sets handler.eof to eof, 60 = 60 second tf (sets bar count properly)
    std::cout << "EOF Found, continuing..." << std::endl;
    int batchSize = 500;
    int i = 0;
    while (true) {
        auto window = engine.handler.requestDataWindow(engine.md, batchSize, 60);
        if (window.prices.empty()) break;
        engine.prices = std::move(window.prices);

        for (int b = 0; b < (int)engine.prices.size(); b++) {
            i++;
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << engine.handler.eof << std::endl; }

            engine.handler.openLong(i);

            float saved = engine.prices.back();
            engine.prices.back() = engine.prices[b];
            engine.handler.tick();
            engine.prices.back() = saved;
        }
    } engine.handler.closeAll();

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
