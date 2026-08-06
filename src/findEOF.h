// finds the last valid row index in a csv using exponential probe + binary search
#pragma once

#include "marketData.h"

inline int findEof(const char* path, int timeframe=1, int strideIncrement=2) {

    auto skipLines = [](MarketData& md, int n) {
        for (int i = 0; i < n; ++i)
            if (!md.skipLine()) return false;
        return true;
    };

    auto lineExists = [&](int row) {
        MarketData md(path);
        return skipLines(md, row + 1);
    };

    MarketData md(path);
    if (!md.skipLine()) return -1;

    int lastValid = 0;
    int stride = 1;

    // phase 1: exponential probing
    while (skipLines(md, stride)) {
        lastValid += stride;
        stride *= strideIncrement;
    }

    int firstInvalid = lastValid + stride;

    // phase 2: binary search the bracket
    while (firstInvalid - lastValid > 1) {
        int midpoint = lastValid + (firstInvalid - lastValid) / 2;
        if (lineExists(midpoint))
            lastValid = midpoint;
        else
            firstInvalid = midpoint;
    }

    if (timeframe <= 1) return lastValid;

    // grab first and last timestamps to compute bar count without a full pass
    MarketData first(path);
    auto firstTick = first.nextTick();
    if (!firstTick) return 0;
    long long startSec = mdDetail::tsToEpochSeconds(firstTick->timestamp);

    MarketData last(path);
    skipLines(last, lastValid);
    auto lastTick = last.nextTick();
    if (!lastTick) return 0;
    long long endSec = mdDetail::tsToEpochSeconds(lastTick->timestamp);

    return static_cast<int>((endSec - startSec) / timeframe);
}
