// set mapping here if building the header manually

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

    // Absolute path to the CSV. Deliberately not CWD-relative: this header
    // gets amalgamated and copied into downstream projects that run from
    // their own directory (not AZBacktest's), so a relative path here would
    // resolve differently - and break - depending on who last regenerated it.
    const char* path;

    DateFormat dateFormat;

    bool skipHeader;

    // if the CSV has multiple symbols interleaved, set these to filter to one
    int symbolCol;          // column index of the symbol field, -1 to disable
    const char* symbol;     // exact symbol, or root prefix if symbolRoll is true
    bool symbolRoll;        // treat symbol as a prefix and auto-roll when the front contract expires

    // Aggressor/side column, read by MarketData::nextTick() to classify each
    // tick's side (Handling::requestDataWindow's execBids/execAsks, and
    // Tick::side). Defaulted off (-1) so existing configs that don't set
    // these still compile and behave exactly as before - only nextTick()
    // (timeframe==0) touches them; nextClose() ignores side entirely.
    int aggressor = -1;                            // column index of the side field, -1 to disable
    const char* buySideAggressorAlias = "B";        // CSV value meaning "buy aggressor / lifted the ask"
    const char* sellSideAggressorAlias = "S";        // CSV value meaning "sell aggressor / hit the bid"
    const char* unknownSideAggressorAlias = "N";     // fallback when the side column doesn't match either alias
};

inline constexpr CSVMapping kCSVMapping{
    PLACEHOLDER_VALUE,              // timestampCol (int)
    PLACEHOLDER_VALUE,              // priceCol (int)
    PLACEHOLDER_VALUE,              // sizeCol (int)
    PLACEHOLDER_VALUE,              // path (const char*)
    { PLACEHOLDER_VALUE },          // dateFormat (DateFormat: yearOff, yearLen, monthOff, monthLen, dayOff, dayLen)
    PLACEHOLDER_VALUE,              // skipHeader (bool)
    PLACEHOLDER_VALUE,              // symbolCol (int, -1 to disable)
    PLACEHOLDER_VALUE,              // symbol (const char*)
    PLACEHOLDER_VALUE,              // symbolRoll (bool)
    PLACEHOLDER_VALUE,              // aggressor (int)
    PLACEHOLDER_VALUE,              // buySideAggressorAlias (const char*)
    PLACEHOLDER_VALUE,              // sellSideAggressorAlias (const char*)
    PLACEHOLDER_VALUE,              // unknownSideAggressorAlias (const char*)
};
