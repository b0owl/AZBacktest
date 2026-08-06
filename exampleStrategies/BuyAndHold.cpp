#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"

int main() {
    // removes some boilerplate code, not necessarily needed but cleaner to use
    auto e = setupEngine(0.25, 0.50); // tickSize & tickValue

    std::cout << "Fetching EOF..." << std::endl;
    e.h.fetchEOF(60); // sets h.eof to eof, 60 = 60 second tf (sets bar count properly)
    std::cout << "EOF Found, continuing..." << std::endl;
    int batchSize = 500;
    int i = 0;
    while (true) {
        auto window = e.h.requestDataWindow(e.md, batchSize, 60);
        if (window.prices.empty()) break;
        e.prices = std::move(window.prices);

        for (int b = 0; b < (int)e.prices.size(); b++) {
            i++;
            if (i % 5000 == 0) { std::cout << "  bar " << i << " / " << e.h.eof << std::endl; }

            e.h.openLong(i);

            float saved = e.prices.back();
            e.prices.back() = e.prices[b];
            e.h.tick();
            e.prices.back() = saved;
        }
    } e.h.closeAll();

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

    showConsole("Console");
}
