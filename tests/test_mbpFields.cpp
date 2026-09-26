// Unit tests for the MBP extras on Tick (flags, bid/ask order counts,
// instrument id, symbol) and for the empty-symbol "read but don't filter" rule.
// CSV only, same as the rest of the suite.

#include "tests/testFramework.h"
#include "tests/fakeData.h"

#include "src/marketData.h"

using azt::TempCsv;
using azt::useFixtureMapping;

// the canonical fixture plus flags, bid_ct, ask_ct, instrument_id on the end
static std::string mbpTicks() {
    return std::string("ts,symbol,price,size,side,bidsz,asksz,bidpx,askpx,action,flags,bidct,askct,iid\n") +
        "2026-06-21T22:00:00.202646469Z,NQU6,30560.25,1,A,4,2,30560.25,30578.75,T,0,3,1,42\n"
        "2026-06-21T22:00:00.202646469Z,NQU6,30550.25,2,A,4,2,30560.25,30578.75,T,0,3,1,42\n"
        "2026-06-21T22:00:00.202646469Z,NQZ6,30550.25,2,B,2,7,30550.00,30578.75,C,128,1,5,77\n";
}

static void useMbpMapping() {
    useFixtureMapping();
    kCSVMapping.flagsCol      = 10;
    kCSVMapping.bidCountCol   = 11;
    kCSVMapping.askCountCol   = 12;
    kCSVMapping.instrumentCol = 13;
    kCSVMapping.symbolCol     = 1;
    kCSVMapping.symbol        = "";
}

TEST(nextTickCarriesMbpExtras) {
    useMbpMapping();
    TempCsv csv(mbpTicks());
    MarketData md(csv.path());

    auto t0 = md.nextTick();
    REQUIRE(t0.has_value());
    CHECK_EQ(t0->flags, 0);
    CHECK_F(t0->bidCount, 3.0);
    CHECK_F(t0->askCount, 1.0);
    CHECK_EQ(t0->instrumentId, 42LL);
    CHECK(t0->symbol == "NQU6");

    md.nextTick();
    auto t2 = md.nextTick();
    REQUIRE(t2.has_value());
    CHECK_EQ(t2->flags, 128);
    CHECK_F(t2->askCount, 5.0);
    CHECK_EQ(t2->instrumentId, 77LL);
    CHECK(t2->symbol == "NQZ6");
}

// an empty symbol reads the column without filtering, so both contracts come through
TEST(emptySymbolReadsEveryRow) {
    useMbpMapping();
    TempCsv csv(mbpTicks());
    MarketData md(csv.path());
    int n = 0;
    while (md.nextTick()) n++;
    CHECK_EQ(n, 3);
}

TEST(exactSymbolStillFilters) {
    useMbpMapping();
    kCSVMapping.symbol = "NQZ6";
    TempCsv csv(mbpTicks());
    MarketData md(csv.path());
    auto t = md.nextTick();
    REQUIRE(t.has_value());
    CHECK(t->symbol == "NQZ6");
    CHECK(!md.nextTick().has_value());
}

TEST(extrasDefaultWhenDisabled) {
    useFixtureMapping();
    TempCsv csv(mbpTicks());
    MarketData md(csv.path());
    auto t = md.nextTick();
    REQUIRE(t.has_value());
    CHECK_EQ(t->flags, 0);
    CHECK_F(t->bidCount, 0.0);
    CHECK_EQ(t->instrumentId, 0LL);
    CHECK(t->symbol.empty());
}
