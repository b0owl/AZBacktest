// this isnt a strategy
// its just a window (as the name implies) for style-dev purposes 
// also good for understanding the gui api

#include <iostream>
#include <vector>

#include "../src/backtestApi.h"
#include "../src/initSeries.h"
#include "../src/window/window.h"
#include "../src/skins/light.h"
#include "../src/skins/dark.h"

int main() {
    initSeriesPool(
        {{1,2,3,1,2,6,3,2,6,4,3,2,1,8,4}, {1,2,3,4}},
        {"1", "2"},
        {0, 1}
    );
    addSeries("3", std::vector<std::vector<float>>{{1,2,3,4},{7,5,4,9}}, {"Cool Custom Name 1", "Cool Custom Name 2"}); // initSeriesPool only supports 1d

    addHeatmap("heatmap demo", std::vector<float>{
        1, 2, 3, 4, 5,
        6, 7, 8, 9, 10,
        11, 12, 13, 14, 15,
        16, 17, 18, 19, 20
    }, 4, 5);

    addStat("bananas per second", 42.0f);
    addStat("moon distance (km)", 384400.0f);
    addStat("vibe score", 0.87f);
    addStat("ducks in a row", 7.0f);
    addStat("pi (approx)", 3.14159f);

    std::cout << "Thanks for trying out my project :)\n"
              << "You can change the theme by calling showConsole with the appropriate function type "
                 "(see the bottom of any example file); all normal ImGUI attributes are customizeable.\n";

    showConsole("Console", skins::light);
    // showConsole("Console", skins::dark);
}
