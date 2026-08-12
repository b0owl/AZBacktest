#include "window/tooling/seriesPool.h"
#include "window/tooling/statPool.h"

// re-export so strategies can use these without namespace qualifiers
using seriesPool::RGBA;
using seriesPool::addSeries;
using seriesPool::addXYBars;
using seriesPool::addHeatmap;
using seriesPool::initSeriesPool;
using statPool::addStat;
