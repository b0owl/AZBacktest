/// Mapping / path config
// Default settings are for *MY* codebase !!!
// The actual CSV index maps are for Databentos TBBO Schema.
// The fastest way to config your mapping here would be to ask claude,
// but it won't take long to do manually.
// be weary of misconfiguration, though.

// You can set values to none if they dont exist in the CSV, just don't call anything that needs them. 

#pragma once

// Substring offsets used to pull Y/M/D out of the timestamp column.
// Defaults match Databento's ISO-8601 UTC nanosecond format:
//   2025-06-01T22:00:00.065308005Z
//    ^^^^ ^^ ^^
//    year mo day
struct DateFormat {
    int yearOffset;
    int yearLength;
    int monthOffset;
    int monthLength;
    int dayOffset;
    int dayLength;
};

struct CSVMapping {
    int timestampCol;
    int priceCol;
    int sizeCol;

    // Path to the CSV, relative to the CWD from which the binary runs.
    const char* path;

    DateFormat dateFormat;

    bool skipHeader;

    // if the CSV has multiple symbols interleaved, set these to filter to one
    int symbolCol;          // column index of the symbol field, -1 to disable
    const char* symbol;     // exact symbol, or root prefix if symbolRoll is true
    bool symbolRoll;        // treat symbol as a prefix and auto-roll when the front contract expires
};

inline constexpr CSVMapping kCSVMapping{
    0,                                                    // timestampCol
    8,                                                    // priceCol
    9,                                                    // size, aka volume at that tick
    "Data/glbx-mdp3-20250601-20260514.tbbo.csv",          // path
    { 0, 4, 5, 2, 8, 2 },                                 // dateFormat: Y off/len, M off/len, D off/len
    true,                                                 // skipHeader
    19,                                                    // symbolCol
    "MNQ",                                                 // symbol (root prefix for rolling)
    true,                                                  // symbolRoll
};
