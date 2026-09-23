// Unit tests for action-column classification: mdDetail::classifyAction and
// its wiring into MarketData::nextTick/nextClose. Parquet isn't covered here,
// same limitation as the rest of this suite (see README.md's Tests section) -
// the Parquet action wiring was checked separately via a direct compile+link.

#include "tests/testFramework.h"
#include "tests/fakeData.h"

#include "src/marketData.h"

using azt::TempCsv;
using azt::useFixtureMapping;

// ---------------------------------------------------------------- classifyAction

TEST(classifyActionMatchesEachConfiguredAlias) {
    useFixtureMapping();
    CHECK_EQ(mdDetail::classifyAction("A"), kCSVMapping.actionAddAlias);
    CHECK_EQ(mdDetail::classifyAction("C"), kCSVMapping.actionCancelAlias);
    CHECK_EQ(mdDetail::classifyAction("M"), kCSVMapping.actionModifyAlias);
    CHECK_EQ(mdDetail::classifyAction("T"), kCSVMapping.actionTradeAlias);
    CHECK_EQ(mdDetail::classifyAction("F"), kCSVMapping.actionFillAlias);
    CHECK_EQ(mdDetail::classifyAction("R"), kCSVMapping.actionClearAlias);
    CHECK_EQ(mdDetail::classifyAction("N"), kCSVMapping.actionNoneAlias);
}

// anything that isn't one of the seven aliases lands on actionNoneAlias, same
// "unknown lands on the neutral default" rule the aggressor split uses
TEST(classifyActionFallsBackToNoneAliasOnUnmatchedValue) {
    useFixtureMapping();
    CHECK_EQ(mdDetail::classifyAction("Z"), kCSVMapping.actionNoneAlias);
    CHECK_EQ(mdDetail::classifyAction(""), kCSVMapping.actionNoneAlias);
}

// ---------------------------------------------------------------- nextTick

TEST(nextTickClassifiesEveryActionAlias) {
    useFixtureMapping();
    TempCsv csv(azt::basicTicks());
    MarketData md(csv.path());

    // basicTicks() rows carry A,C,M,T,F,R in that order
    const char* want[6] = {
        kCSVMapping.actionAddAlias,    kCSVMapping.actionCancelAlias,
        kCSVMapping.actionModifyAlias, kCSVMapping.actionTradeAlias,
        kCSVMapping.actionFillAlias,   kCSVMapping.actionClearAlias,
    };
    for (int i = 0; i < 6; i++) {
        auto t = md.nextTick();
        REQUIRE(t.has_value());
        CHECK_EQ(t->action, want[i]);
    }
}

TEST(nextTickActionDefaultsToNoneAliasWhenDisabled) {
    useFixtureMapping();
    kCSVMapping.actionCol = -1;
    TempCsv csv(azt::basicTicks());
    MarketData md(csv.path());

    auto t = md.nextTick(); // raw column says A, but classification is off
    REQUIRE(t.has_value());
    CHECK_EQ(t->action, kCSVMapping.actionNoneAlias);
}

// ---------------------------------------------------------------- nextClose

// action is a book-event classification, not something that sums or averages
// across a bar, so a bar reports the closing row's action - bar 1's rows carry
// A,C,M,T, if this ever starts reporting the first row it would read A instead
TEST(nextCloseReportsClosingRowAction) {
    useFixtureMapping();
    TempCsv csv(azt::basicTicks());
    MarketData md(csv.path());

    auto bar1 = md.nextClose(60);
    REQUIRE(bar1.has_value());
    CHECK_EQ(bar1->action, kCSVMapping.actionTradeAlias); // row 3, the bar's last row

    auto bar2 = md.nextClose(60);
    REQUIRE(bar2.has_value());
    CHECK_EQ(bar2->action, kCSVMapping.actionClearAlias); // row 5, the bar's last row
}

TEST(nextCloseActionDefaultsToNoneAliasWhenDisabled) {
    useFixtureMapping();
    kCSVMapping.actionCol = -1;
    TempCsv csv(azt::basicTicks());
    MarketData md(csv.path());

    auto bar = md.nextClose(60);
    REQUIRE(bar.has_value());
    CHECK_EQ(bar->action, kCSVMapping.actionNoneAlias);
    CHECK_F(bar->size, 17.0); // aggregation still works
}
