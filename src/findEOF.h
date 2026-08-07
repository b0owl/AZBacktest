// finds the last valid row index in a csv using exponential probe + binary search
#pragma once

#include "marketData.h"

inline int findEof(const char* path, int timeframe=1, int strideIncrement=2) {

    auto skipLines = [](MarketData& md, int n) {
        for (int i = 0; i < n; ++i)
            if (!md.skipLine()) return false;
        return true;
    };

    MarketData md(path);
    if (!md.skipLine()) return -1;

    int lastValid = 0;
    int stride = 1;
    std::size_t lastValidOff = md.byteOffset();

    // phase 1: exponential probing (single sequential pass)
    while (skipLines(md, stride)) {
        lastValid += stride;
        lastValidOff = md.byteOffset();
        stride *= strideIncrement;
    }

    int firstInvalid = lastValid + stride;

    // phase 2: binary search, seek from lastValid's byte offset each probe
    while (firstInvalid - lastValid > 1) {
        int midpoint = lastValid + (firstInvalid - lastValid) / 2;
        int delta = midpoint - lastValid;

        md.seekTo(lastValidOff);
        if (skipLines(md, delta)) {
            lastValid = midpoint;
            lastValidOff = md.byteOffset();
        } else {
            firstInvalid = midpoint;
        }
    }

    if (timeframe <= 1) return lastValid;

    // grab first and last timestamps to compute bar count without a full pass
    MarketData first(path);
    auto firstTick = first.nextTick();
    if (!firstTick) return 0;
    long long startSec = mdDetail::tsToEpochSeconds(firstTick->timestamp);

    md.seekTo(lastValidOff);
    auto lastTick = md.nextTick();
    if (!lastTick) return 0;
    long long endSec = mdDetail::tsToEpochSeconds(lastTick->timestamp);

    return static_cast<int>((endSec - startSec) / timeframe);
}
