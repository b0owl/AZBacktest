// Tiny assertion + registration framework for the AZBacktest test suite.
//
// Deliberately header-only and dependency-free, matching the rest of the
// project. It also defines main(), which is only safe because every test file
// is compiled into its OWN binary - backtestApi.h declares `trades`,
// `realizedProfit` and `equityCurve` as non-inline globals, so two test TUs
// that both include it would collide at link time anyway. build.sh -tests
// builds and runs each tests/test_*.cpp separately for that reason, which has
// the nice side effect of isolating the global kCSVMapping per file.
//
// Usage:
//   #include "tests/testFramework.h"
//   TEST(somethingWorks) {
//       CHECK_EQ(2 + 2, 4);
//   }
// no main() needed, this header supplies it

#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace azt {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

// failures recorded by the test currently running, reset before each one
inline int& currentFailures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

// value printing for assertion messages, arithmetic types go through
// to_string, everything stringy gets quoted so empty vs missing is visible
inline std::string toStr(bool v)             { return v ? "true" : "false"; }
inline std::string toStr(const std::string& v)  { return "\"" + v + "\""; }
inline std::string toStr(std::string_view v)    { return "\"" + std::string(v) + "\""; }
inline std::string toStr(const char* v)         { return v ? "\"" + std::string(v) + "\"" : "(null)"; }
template <typename T>
inline std::enable_if_t<std::is_arithmetic_v<T>, std::string> toStr(T v) { return std::to_string(v); }

inline void reportFailure(const char* file, int line, const std::string& msg) {
    currentFailures()++;
    std::printf("        %s:%d: %s\n", file, line, msg.c_str());
    std::fflush(stdout);
}

/// @brief run every registered test, printing a line per test plus a summary
/// @return 0 if all passed, 1 otherwise (so the shell can gate on it)
inline int runAll(const char* suite) {
    auto& tests = registry();
    std::printf("running %zu test(s) in %s\n", tests.size(), suite);

    int passed = 0, failed = 0;
    for (const auto& t : tests) {
        currentFailures() = 0;
        t.fn();
        // failures print themselves as they happen, so the FAIL line lands
        // underneath its own detail lines rather than above them
        if (currentFailures() == 0) {
            std::printf("  PASS  %s\n", t.name);
            passed++;
        } else {
            std::printf("  FAIL  %s (%d assertion(s))\n", t.name, currentFailures());
            failed++;
        }
        std::fflush(stdout);
    }

    std::printf("%s: %d test(s), %d passed, %d failed\n\n", suite, (int)tests.size(), passed, failed);
    return failed == 0 ? 0 : 1;
}

} // namespace azt

/// @brief define a test, the body runs with no arguments and reports through
/// the CHECK_* macros below
#define TEST(name)                                              \
    static void name();                                         \
    static azt::Registrar _aztReg_##name(#name, name);          \
    static void name()

/// @brief non-fatal boolean assertion, the test keeps going so one run can
/// surface every problem in the case rather than just the first
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) azt::reportFailure(__FILE__, __LINE__,                    \
            std::string("CHECK(") + #cond + ") failed");                       \
    } while (0)

/// @brief fatal version of CHECK, bails out of the test on failure. use it
/// before dereferencing an optional so a miss doesn't turn into a crash
#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            azt::reportFailure(__FILE__, __LINE__,                             \
                std::string("REQUIRE(") + #cond + ") failed, aborting test");  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define CHECK_EQ(got, want)                                                    \
    do {                                                                       \
        auto _g = (got); auto _w = (want);                                     \
        if (!(_g == _w)) azt::reportFailure(__FILE__, __LINE__,                \
            std::string(#got) + " -> got " + azt::toStr(_g)                    \
                              + ", want " + azt::toStr(_w));                   \
    } while (0)

#define CHECK_NE(got, unwanted)                                                \
    do {                                                                       \
        auto _g = (got); auto _u = (unwanted);                                 \
        if (_g == _u) azt::reportFailure(__FILE__, __LINE__,                   \
            std::string(#got) + " -> got " + azt::toStr(_g)                    \
                              + ", wanted anything else");                     \
    } while (0)

/// @brief float comparison with a tolerance, prices and volumes go through
/// from_chars into floats so exact equality is the wrong tool
#define CHECK_NEAR(got, want, eps)                                             \
    do {                                                                       \
        double _g = (double)(got), _w = (double)(want);                        \
        if (!(std::fabs(_g - _w) <= (eps))) azt::reportFailure(__FILE__, __LINE__, \
            std::string(#got) + " -> got " + azt::toStr(_g)                    \
                              + ", want " + azt::toStr(_w)                     \
                              + " (+/- " + azt::toStr((double)(eps)) + ")");   \
    } while (0)

/// @brief CHECK_NEAR with the tolerance every price/volume check in this suite
/// uses, tight enough to catch a real parse bug, loose enough for float
#define CHECK_F(got, want) CHECK_NEAR(got, want, 1e-4)

// build.sh -tests passes the suite name in, a file compiled by hand still works
#ifndef AZT_SUITE_NAME
#define AZT_SUITE_NAME "tests"
#endif

int main() { return azt::runAll(AZT_SUITE_NAME); }
