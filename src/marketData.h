// I genuinely have no clue how to do this file stuff
// so claude did it for me... sorry
// yes i am that lazy
// be thankful i opensourced this for yall

#pragma once

#include "dataConfig.h"

#include <charconv>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

/// @brief a single row from the CSV, timestamp + price as string_views into
/// the mmapped buffer (only valid while MarketData is alive), plus a parsed
/// volume. for nextTick, `size` is that tick's traded size; for nextClose,
/// it's the summed volume across every tick in the bar
///
/// `timestamp`, `price` and `side` always describe a single row (the tick
/// itself, or the bar's closing tick for nextClose). `size` and the three
/// volume splits are aggregates over the whole bar when it came from nextClose.
///
/// executedBuys/executedSells split `size` by aggressor: buy-side volume is a
/// buyer lifting the ask, sell-side volume is a seller hitting the bid. Rows
/// whose side column matches neither alias (or when `aggressor` is -1 in the
/// config) land in unknownVolume. The three always sum to `size`.
struct Tick {
    std::string_view timestamp;
    std::string_view price;
    float size = 0.f;
    const char* side = kCSVMapping.unknownSideAggressorAlias;
    float executedBuys   = 0.f;
    float executedSells  = 0.f;
    float unknownVolume  = 0.f;
};

namespace mdDetail {

/// @brief Howard Hinnant's days-from-civil algorithm, portable, no DST/locale issues
/// converts a Y/M/D date to a day count since the Unix epoch
inline long long civilToDays(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + static_cast<long long>(doe) - 719468;
}

struct CivilDate { int y; unsigned m; unsigned d; };

/// @brief inverse of civilToDays, epoch day count back to Y/M/D
inline CivilDate daysToCivil(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    const unsigned d   = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m   = mp < 10 ? mp + 3 : mp - 9;
    return { y + (m <= 2 ? 1 : 0), m, d };
}

/// @brief parse an ISO-8601 timestamp string into epoch seconds
/// Y/M/D offsets come from dataConfig so different CSV formats just work
/// H/M/S offsets are hardcoded (they don't move around in ISO-8601)
/// @param ts the raw timestamp string_view from the CSV row
inline long long tsToEpochSeconds(std::string_view ts) {
    auto toInt = [](const char* p, int n) {
        int v = 0;
        std::from_chars(p, p + n, v);
        return v;
    };
    int y  = toInt(ts.data() + kCSVMapping.dateFormat.yearOffset,  kCSVMapping.dateFormat.yearLength);
    int mo = toInt(ts.data() + kCSVMapping.dateFormat.monthOffset, kCSVMapping.dateFormat.monthLength);
    int d  = toInt(ts.data() + kCSVMapping.dateFormat.dayOffset,   kCSVMapping.dateFormat.dayLength);
    int h  = toInt(ts.data() + 11, 2);
    int mi = toInt(ts.data() + 14, 2);
    int s  = toInt(ts.data() + 17, 2);
    return civilToDays(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400LL
         + h * 3600LL + mi * 60LL + s;
}

/// @brief build an ISO-8601 string for `startTs + seconds`, since ISO-8601
/// sorts lexicographically, nextClose can just do string compares against
/// this instead of parsing every row, way cheaper (~19-byte memcmp)
/// @param startTs the starting timestamp to offset from
/// @param seconds how far forward to go
inline std::string endTimestamp(std::string_view startTs, int seconds) {
    long long target = tsToEpochSeconds(startTs) + seconds;
    long long days = target / 86400;
    long long tod  = target % 86400;
    if (tod < 0) { tod += 86400; --days; }
    int h  = static_cast<int>(tod / 3600);
    int mi = static_cast<int>((tod / 60) % 60);
    int s  = static_cast<int>(tod % 60);
    CivilDate c = daysToCivil(days);
    char buf[24];
    int n = std::snprintf(buf, sizeof(buf),
        "%04d-%02u-%02uT%02d:%02d:%02d", c.y, c.m, c.d, h, mi, s);
    return std::string(buf, static_cast<size_t>(n));
}

/// @brief inverse of tsToEpochSeconds' string slicing: format epoch nanoseconds
/// back into Databento's ISO-8601 layout (e.g. "2025-06-01T22:00:00.065308005Z"),
/// byte-for-byte compatible with the CSV's raw format. Used by the Parquet
/// backend so Tick::timestamp stays a string_view either way, letting every
/// existing consumer (tsToEpochSeconds, endTimestamp) work unmodified.
/// @param buf caller-owned buffer, must be at least 32 bytes
/// @return number of bytes written (excluding the null terminator)
inline int formatIsoTimestamp(char* buf, std::size_t bufSize, long long nanosSinceEpoch) {
    long long days = nanosSinceEpoch / 86400000000000LL;
    long long rem  = nanosSinceEpoch % 86400000000000LL;
    if (rem < 0) { rem += 86400000000000LL; --days; }
    long long secOfDay = rem / 1000000000LL;
    long long fracNs    = rem % 1000000000LL;
    int h  = static_cast<int>(secOfDay / 3600);
    int mi = static_cast<int>((secOfDay / 60) % 60);
    int s  = static_cast<int>(secOfDay % 60);
    CivilDate c = daysToCivil(days);
    return std::snprintf(buf, bufSize, "%04d-%02u-%02uT%02d:%02d:%02d.%09lldZ",
        c.y, c.m, c.d, h, mi, s, fracNs);
}

/// @brief find first '\n' in [p, end), returns end if none found
/// memchr on modern glibc / MSVC uses SIMD (SSE2/AVX) for the scan so
/// this is basically free compared to looping byte-by-byte
inline const char* findEOL(const char* p, const char* end) {
    if (p >= end) return end;
    const void* nl = std::memchr(p, '\n', static_cast<size_t>(end - p));
    return nl ? static_cast<const char*>(nl) : end;
}

/// @brief grab the col-th (0-indexed) comma-separated field from a CSV row
/// assumes no quoted fields, fine for the numeric TBBO schema
/// @param start beginning of the row
/// @param end   one past the last char of the row (newline or EOF)
/// @param col   which column to extract
inline std::string_view field(const char* start, const char* end, int col) {
    const char* p = start;
    for (int i = 0; i < col; ++i) {
        if (p >= end) return {};
        const void* c = std::memchr(p, ',', static_cast<size_t>(end - p));
        if (!c) return {};
        p = static_cast<const char*>(c) + 1;
    }
    const void* c = (p < end)
        ? std::memchr(p, ',', static_cast<size_t>(end - p))
        : nullptr;
    const char* fieldEnd = c ? static_cast<const char*>(c) : end;
    return std::string_view(p, static_cast<size_t>(fieldEnd - p));
}

/// @brief extract two fields (cols c1 and c2, c1 <= c2) in a single pass
/// instead of calling field() twice. used by nextTick since it always
/// needs both timestamp and price from the same row
/// @param c1 first column index
/// @param c2 second column index (must be >= c1)
/// @param f1 output: view of the first field
/// @param f2 output: view of the second field
inline void twoFields(const char* start, const char* end, int c1, int c2,
                      std::string_view& f1, std::string_view& f2) {
    const char* p  = start;
    const char* fs = start;
    int col = 0;
    while (p < end) {
        if (*p == ',') {
            if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c2) { f2 = std::string_view(fs, static_cast<size_t>(p - fs)); return; }
            ++col;
            fs = p + 1;
        }
        ++p;
    }
    if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c2) f2 = std::string_view(fs, static_cast<size_t>(p - fs));
}

/// @brief three-field variant of twoFields, single-pass extraction of cols
/// c1 <= c2 <= c3, used by nextTick which needs timestamp + price + size
inline void threeFields(const char* start, const char* end, int c1, int c2, int c3,
                        std::string_view& f1, std::string_view& f2, std::string_view& f3) {
    const char* p  = start;
    const char* fs = start;
    int col = 0;
    while (p < end) {
        if (*p == ',') {
            if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c2) f2 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c3) { f3 = std::string_view(fs, static_cast<size_t>(p - fs)); return; }
            ++col;
            fs = p + 1;
        }
        ++p;
    }
    if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c2) f2 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c3) f3 = std::string_view(fs, static_cast<size_t>(p - fs));
}

/// @brief four-field variant, single-pass extraction of cols c1 <= c2 <= c3 <= c4
inline void fourFields(const char* start, const char* end, int c1, int c2, int c3, int c4,
                       std::string_view& f1, std::string_view& f2, std::string_view& f3, std::string_view& f4) {
    const char* p  = start;
    const char* fs = start;
    int col = 0;
    while (p < end) {
        if (*p == ',') {
            if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c2) f2 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c3) f3 = std::string_view(fs, static_cast<size_t>(p - fs));
            if (col == c4) { f4 = std::string_view(fs, static_cast<size_t>(p - fs)); return; }
            ++col;
            fs = p + 1;
        }
        ++p;
    }
    if (col == c1) f1 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c2) f2 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c3) f3 = std::string_view(fs, static_cast<size_t>(p - fs));
    if (col == c4) f4 = std::string_view(fs, static_cast<size_t>(p - fs));
}

} // namespace mdDetail

// Parquet-backed reader (_ParquetMarketData), compiled in only when build.sh
// detects Arrow/Parquet on the system (defines AZBT_PARQUET). Needs Tick and
// mdDetail::* from above, so it's included here rather than from the top of
// this file. A no-op include when AZBT_PARQUET isn't defined.
#include "parquetMarketData.h"

/// @brief streaming reader over a raw byte range, used internally by MarketData
class _MarketData {
private:
    const char* _base;
    const char* _cur;
    const char* _end;
    bool _consumedHeader = false;

    // contract roll state
    std::string _activeSymbol;
    int _missCount = 0;
    static constexpr int _rollThreshold = 200;

    void _skipHeaderOnce() {
        if (_consumedHeader) return;
        const char* eol = mdDetail::findEOL(_cur, _end);
        _cur = (eol < _end) ? eol + 1 : _end;
        _consumedHeader = true;
    }

    // advance past rows that dont match the configured symbol filter
    // handles both exact match and rolling contract mode
    const char* _nextMatchingLine() {
        if (kCSVMapping.symbolCol < 0) {
            if (_cur < _end) return _cur;
            return nullptr;
        }
        std::string_view root(kCSVMapping.symbol);
        while (_cur < _end) {
            const char* line = _cur;
            const char* eol  = mdDetail::findEOL(line, _end);
            if (line == eol) return nullptr;
            std::string_view sym = mdDetail::field(line, eol, kCSVMapping.symbolCol);

            if (!kCSVMapping.symbolRoll) {
                // exact match mode
                if (sym == root) return line;
                _cur = (eol < _end) ? eol + 1 : _end;
                continue;
            }

            // rolling mode: match root prefix
            if (sym.size() < root.size() || sym.substr(0, root.size()) != root) {
                _cur = (eol < _end) ? eol + 1 : _end;
                continue;
            }

            // first contract we see becomes active
            if (_activeSymbol.empty()) {
                _activeSymbol = std::string(sym);
                _missCount = 0;
                return line;
            }

            // matches the active contract
            if (sym == std::string_view(_activeSymbol)) {
                _missCount = 0;
                return line;
            }

            // different contract, count misses and roll if the active one is gone
            _missCount++;
            if (_missCount >= _rollThreshold) {
                _activeSymbol = std::string(sym);
                _missCount = 0;
                return line;
            }
            _cur = (eol < _end) ? eol + 1 : _end;
        }
        return nullptr;
    }

public:
    _MarketData(const char* data, std::size_t size)
        : _base(data), _cur(data), _end(data + size) {}

    // skip past one line without parsing, returns false at EOF
    bool skipLine() {
        _skipHeaderOnce();
        if (_cur >= _end) return false;
        const char* eol = mdDetail::findEOL(_cur, _end);
        _cur = (eol < _end) ? eol + 1 : _end;
        return true;
    }

    std::size_t byteOffset() const { return static_cast<std::size_t>(_cur - _base); }
    void seekTo(std::size_t off) { _cur = _base + off; _consumedHeader = true; }

    /// @brief next tick from the file (header skipped on first call)
    /// @return the next tick as views into the underlying buffer, or nullopt at EOF
    std::optional<Tick> nextTick() {
        _skipHeaderOnce();
        const char* line = _nextMatchingLine();
        if (!line) return std::nullopt;
        const char* eol = mdDetail::findEOL(line, _end);
        _cur = (eol < _end) ? eol + 1 : _end;
        Tick t;
        std::string_view sideView, szView;
        mdDetail::fourFields(line, eol,
            kCSVMapping.timestampCol, kCSVMapping.aggressor, kCSVMapping.priceCol, kCSVMapping.sizeCol,
            t.timestamp, sideView, t.price, szView);
        std::from_chars(szView.data(), szView.data() + szView.size(), t.size);
        if (kCSVMapping.aggressor >= 0 && sideView == kCSVMapping.buySideAggressorAlias) {
            t.side = kCSVMapping.buySideAggressorAlias;
            t.executedBuys = t.size;
        } else if (kCSVMapping.aggressor >= 0 && sideView == kCSVMapping.sellSideAggressorAlias) {
            t.side = kCSVMapping.sellSideAggressorAlias;
            t.executedSells = t.size;
        } else {
            t.unknownVolume = t.size;
        }
        return t;
    }

    /// @brief close tick of the next window covering at least `seconds`,
    /// t.size is the summed volume across every tick that fell inside the bar,
    /// split across t.executedBuys / t.executedSells / t.unknownVolume by the
    /// aggressor column. t.side is the side of the closing tick specifically
    /// (same row the timestamp and price come from), not the bar as a whole
    /// @return the close tick (views into the underlying buffer), or nullopt at EOF
    std::optional<Tick> nextClose(int seconds) {
        _skipHeaderOnce();
        const char* line = _nextMatchingLine();
        if (!line) return std::nullopt;
        const char* eol = mdDetail::findEOL(line, _end);
        _cur = (eol < _end) ? eol + 1 : _end;

        std::string_view firstTs = mdDetail::field(line, eol, kCSVMapping.timestampCol);
        std::string targetOwned  = mdDetail::endTimestamp(firstTs, seconds);
        std::string_view target(targetOwned);

        float barVolume = 0.f, buys = 0.f, sells = 0.f, unknown = 0.f;
        // the row's side alias, or nullptr when side classification is disabled
        const char* rowSide = nullptr;

        // parse one row's size and fold it into the running per-side totals
        auto accumulate = [&](const char* l, const char* e) {
            std::string_view szView = mdDetail::field(l, e, kCSVMapping.sizeCol);
            float sz = 0.f;
            std::from_chars(szView.data(), szView.data() + szView.size(), sz);
            barVolume += sz;

            rowSide = nullptr;
            if (kCSVMapping.aggressor >= 0) {
                std::string_view sideView = mdDetail::field(l, e, kCSVMapping.aggressor);
                if (sideView == kCSVMapping.buySideAggressorAlias)       rowSide = kCSVMapping.buySideAggressorAlias;
                else if (sideView == kCSVMapping.sellSideAggressorAlias) rowSide = kCSVMapping.sellSideAggressorAlias;
            }
            if (rowSide == kCSVMapping.buySideAggressorAlias)       buys  += sz;
            else if (rowSide == kCSVMapping.sellSideAggressorAlias) sells += sz;
            else                                                    unknown += sz;
        };

        accumulate(line, eol);

        const char* lastLine = line;
        const char* lastEol  = eol;
        std::string_view lastTs = firstTs;

        while (lastTs < target) {
            const char* nline = _nextMatchingLine();
            if (!nline) break;
            const char* neol = mdDetail::findEOL(nline, _end);
            _cur = (neol < _end) ? neol + 1 : _end;

            lastLine = nline;
            lastEol  = neol;
            lastTs   = mdDetail::field(nline, neol, kCSVMapping.timestampCol);
            accumulate(nline, neol);
        }

        Tick t;
        t.timestamp = lastTs;
        t.price     = mdDetail::field(lastLine, lastEol, kCSVMapping.priceCol);
        t.size      = barVolume;
        // rowSide is left pointing at the last row accumulate() saw, i.e. the close
        if (rowSide) t.side = rowSide;
        t.executedBuys  = buys;
        t.executedSells = sells;
        t.unknownVolume = unknown;
        return t;
    }
};

/// @brief true if `path` ends in ".parquet" (case-sensitive), used by MarketData
/// to decide which backend to construct
inline bool isParquetPath(const std::string& path) {
    static constexpr std::string_view kExt = ".parquet";
    return path.size() >= kExt.size()
        && path.compare(path.size() - kExt.size(), kExt.size(), kExt) == 0;
}

/// @brief reads market data from either a memory-mapped CSV or (when built
/// with Arrow/Parquet support, see build.sh) a Parquet file, picked by the
/// extension of `path`. Tick fields are views into per-instance buffers,
/// valid until the next call on this MarketData instance.
class MarketData {
private:
    struct RawMap {
        const char* data = nullptr;
        std::size_t size = 0;
#ifdef _WIN32
        HANDLE hFile = INVALID_HANDLE_VALUE;
        HANDLE hMap  = nullptr;
#else
        int fd = -1;
#endif
    };
    RawMap _map;
    std::unique_ptr<_MarketData> _csv;
#ifdef AZBT_PARQUET
    std::unique_ptr<_ParquetMarketData> _pq;
#endif

    static RawMap openMap(const std::string& path) {
        RawMap m;
#ifdef _WIN32
        m.hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
        if (m.hFile == INVALID_HANDLE_VALUE)
            throw std::runtime_error("MarketData: open failed: " + path);
        LARGE_INTEGER sz;
        if (!GetFileSizeEx(m.hFile, &sz)) {
            CloseHandle(m.hFile);
            throw std::runtime_error("MarketData: size query failed: " + path);
        }
        m.size = static_cast<std::size_t>(sz.QuadPart);
        m.hMap = CreateFileMappingA(m.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!m.hMap) {
            CloseHandle(m.hFile);
            throw std::runtime_error("MarketData: file mapping failed: " + path);
        }
        void* p = MapViewOfFile(m.hMap, FILE_MAP_READ, 0, 0, 0);
        if (!p) {
            CloseHandle(m.hMap);
            CloseHandle(m.hFile);
            throw std::runtime_error("MarketData: map view failed: " + path);
        }
        m.data = static_cast<const char*>(p);
#else
        m.fd = ::open(path.c_str(), O_RDONLY);
        if (m.fd == -1)
            throw std::runtime_error("MarketData: open failed: " + path);
        struct stat st;
        if (fstat(m.fd, &st) == -1) {
            ::close(m.fd);
            throw std::runtime_error("MarketData: stat failed: " + path);
        }
        m.size = static_cast<std::size_t>(st.st_size);
        void* p = mmap(nullptr, m.size, PROT_READ, MAP_PRIVATE, m.fd, 0);
        if (p == MAP_FAILED) {
            ::close(m.fd);
            throw std::runtime_error("MarketData: mmap failed: " + path);
        }
        madvise(p, m.size, MADV_SEQUENTIAL);
        m.data = static_cast<const char*>(p);
#endif
        return m;
    }

public:
    /// @brief opens `path`, mmapping it if it's a CSV or (given Arrow/Parquet
    /// support, see build.sh) opening it as Parquet if the extension is
    /// ".parquet"
    MarketData(const std::string& path) {
        if (isParquetPath(path)) {
#ifdef AZBT_PARQUET
            _pq = std::make_unique<_ParquetMarketData>(path);
#else
            throw std::runtime_error(
                "MarketData: " + path + " is a .parquet file, but this build has no "
                "Parquet support (Arrow wasn't found when it was built). Install it with "
                "`brew install apache-arrow` and rebuild.");
#endif
        } else {
            _map = openMap(path);
            _csv = std::make_unique<_MarketData>(_map.data, _map.size);
        }
    }

    ~MarketData() {
        if (!_csv) return;
#ifdef _WIN32
        if (_map.data)                        UnmapViewOfFile(_map.data);
        if (_map.hMap)                        CloseHandle(_map.hMap);
        if (_map.hFile != INVALID_HANDLE_VALUE) CloseHandle(_map.hFile);
#else
        if (_map.data) munmap(const_cast<char*>(_map.data), _map.size);
        if (_map.fd != -1) ::close(_map.fd);
#endif
    }

    // copying would double-free the mapping
    MarketData(const MarketData&)            = delete;
    MarketData& operator=(const MarketData&) = delete;

    bool skipLine() {
#ifdef AZBT_PARQUET
        if (_pq) return _pq->skipLine();
#endif
        return _csv->skipLine();
    }

    std::size_t byteOffset() const {
#ifdef AZBT_PARQUET
        if (_pq) return _pq->byteOffset();
#endif
        return _csv->byteOffset();
    }

    void seekTo(std::size_t off) {
#ifdef AZBT_PARQUET
        if (_pq) { _pq->seekTo(off); return; }
#endif
        _csv->seekTo(off);
    }

    /// @brief next raw tick, returns nullopt at EOF
    std::optional<Tick> nextTick() {
#ifdef AZBT_PARQUET
        if (_pq) return _pq->nextTick();
#endif
        return _csv->nextTick();
    }

    /// @brief close tick of the next bar spanning at least `seconds`,
    /// skips forward until the timestamp is >= start + seconds, then
    /// returns that row's price as the "close"
    /// @param seconds bar width in seconds (e.g. 60 for 1-min bars)
    std::optional<Tick> nextClose(int seconds) {
#ifdef AZBT_PARQUET
        if (_pq) return _pq->nextClose(seconds);
#endif
        return _csv->nextClose(seconds);
    }
};
