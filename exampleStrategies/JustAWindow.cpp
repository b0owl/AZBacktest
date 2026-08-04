// this isnt a strategy
// its just a window (as the name implies) for style-dev purposes 
// also good for understanding the gui api

#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"

int main() {
    initSeriesPool(
        {{1,2,3,1,2,6,3,2,6,4,3,2,1,8,4}, {1,2,3,4}},
        {"1", "2"},
        {0, 1}
    );
    addSeries("3", std::vector<std::vector<float>>{{1,2,3,4},{7,5,4,9}}, {"Cool Custom Name 1", "Cool Custom Name 2"}); // initSeriesPool only supports 1d
    showConsole("Console");
}
