// column mapping and data path, loaded at runtime from config.toml
// call loadConfig() before using kCSVMapping (Engine/setupEngine do this automatically)

#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include "src/tomlParser.h"

// substring offsets used to pull Y/M/D out of the timestamp column
// defaults match Databento's ISO-8601 UTC nanosecond format:
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

    // absolute path to the data file (CSV or Parquet)
    // use an absolute path so it resolves correctly regardless of
    // which directory the binary runs from
    const char* path;

    DateFormat dateFormat;

    bool skipHeader;

    // if the CSV has multiple symbols interleaved, set these to filter to one
    int symbolCol;          // column index of the symbol field, -1 to disable
    const char* symbol;     // exact symbol, or root prefix if symbolRoll is true
    bool symbolRoll;        // treat symbol as a prefix and auto-roll when the front contract expires

    // aggressor/side column, read by MarketData::nextTick() to classify each
    // tick's side. defaulted off (-1) so configs that dont set these still work
    int aggressor;                                   // column index of the side field, -1 to disable
    const char* buySideAggressorAlias;               // CSV value meaning "buy aggressor / lifted the ask"
    const char* sellSideAggressorAlias;              // CSV value meaning "sell aggressor / hit the bid"
    const char* unknownSideAggressorAlias;           // fallback when the side column doesnt match either alias

    float commision;                                 // commision, pts
    float spread;                                    // spread, pts
    float timingCost;                                // how much do you lose from latency? (pts)
};

// persistent string storage so const char* fields stay valid
namespace cfgDetail {
    inline std::string pathStr;
    inline std::string symbolStr;
    inline std::string buySideStr    = "B";
    inline std::string sellSideStr   = "S";
    inline std::string unknownSideStr = "N";
    inline bool loaded = false;
}

inline CSVMapping kCSVMapping{
    0, 0, 0,
    "",                        // path (empty until loadConfig)
    {0, 4, 5, 2, 8, 2},       // ISO-8601 defaults
    true,                      // skipHeader
    -1, "", false,             // symbol filtering disabled
    -1, "B", "S", "N",        // aggressor disabled, default aliases
    0.f, 0.f, 0.f             // costs
};

inline void generateDefaultConfig(const char* tomlPath) {
    std::ofstream out(tomlPath);
    out <<
R"(# AZBacktest configuration
# fill in your column mapping and data path, then rebuild

# absolute path to the data file (CSV or Parquet)
path = "PLACEHOLDER"

# column indices (0-indexed)
timestampCol = 0
priceCol     = 0
sizeCol      = 0

# set to true if the CSV has a header row to skip
skipHeader = true

# symbol filtering (set symbolCol to -1 to disable)
symbolCol  = -1
symbol     = ""
symbolRoll = false

# aggressor/side classification (set aggressor to -1 to disable)
aggressor                = -1
buySideAggressorAlias    = "B"
sellSideAggressorAlias   = "S"
unknownSideAggressorAlias = "N"

# trading costs (all in pts)
commission = 0.0
spread     = 0.0
timingCost = 0.0

# substring offsets for pulling Y/M/D out of the timestamp column
# defaults match Databento's ISO-8601 format: 2025-06-01T22:00:00.065308005Z
[dateFormat]
yearOffset  = 0
yearLength  = 4
monthOffset = 5
monthLength = 2
dayOffset   = 8
dayLength   = 2
)";
}

inline void loadConfig(const char* tomlPath = "config.toml") {
    if (cfgDetail::loaded) return;
    cfgDetail::loaded = true;

    {
        std::ifstream check(tomlPath);
        if (!check.is_open()) {
            generateDefaultConfig(tomlPath);
            std::cout << "generated " << tomlPath << " with placeholder values, fill it in and rerun" << std::endl;
            std::exit(0);
        }
    }

    auto cfg = toml::parse(tomlPath);

    kCSVMapping.timestampCol = toml::getInt(cfg, "", "timestampCol");
    kCSVMapping.priceCol     = toml::getInt(cfg, "", "priceCol");
    kCSVMapping.sizeCol      = toml::getInt(cfg, "", "sizeCol");

    cfgDetail::pathStr = toml::getString(cfg, "", "path");
    kCSVMapping.path   = cfgDetail::pathStr.c_str();

    kCSVMapping.dateFormat.yearOffset  = toml::getInt(cfg, "dateFormat", "yearOffset", 0);
    kCSVMapping.dateFormat.yearLength  = toml::getInt(cfg, "dateFormat", "yearLength", 4);
    kCSVMapping.dateFormat.monthOffset = toml::getInt(cfg, "dateFormat", "monthOffset", 5);
    kCSVMapping.dateFormat.monthLength = toml::getInt(cfg, "dateFormat", "monthLength", 2);
    kCSVMapping.dateFormat.dayOffset   = toml::getInt(cfg, "dateFormat", "dayOffset", 8);
    kCSVMapping.dateFormat.dayLength   = toml::getInt(cfg, "dateFormat", "dayLength", 2);

    kCSVMapping.skipHeader = toml::getBool(cfg, "", "skipHeader", true);

    kCSVMapping.symbolCol = toml::getInt(cfg, "", "symbolCol", -1);
    cfgDetail::symbolStr  = toml::getString(cfg, "", "symbol");
    kCSVMapping.symbol    = cfgDetail::symbolStr.c_str();
    kCSVMapping.symbolRoll = toml::getBool(cfg, "", "symbolRoll", false);

    kCSVMapping.aggressor = toml::getInt(cfg, "", "aggressor", -1);
    cfgDetail::buySideStr = toml::getString(cfg, "", "buySideAggressorAlias", "B");
    kCSVMapping.buySideAggressorAlias = cfgDetail::buySideStr.c_str();
    cfgDetail::sellSideStr = toml::getString(cfg, "", "sellSideAggressorAlias", "S");
    kCSVMapping.sellSideAggressorAlias = cfgDetail::sellSideStr.c_str();
    cfgDetail::unknownSideStr = toml::getString(cfg, "", "unknownSideAggressorAlias", "N");
    kCSVMapping.unknownSideAggressorAlias = cfgDetail::unknownSideStr.c_str();

    kCSVMapping.commision  = toml::getFloat(cfg, "", "commission", 0.f);
    kCSVMapping.spread     = toml::getFloat(cfg, "", "spread", 0.f);
    kCSVMapping.timingCost = toml::getFloat(cfg, "", "timingCost", 0.f);
}
