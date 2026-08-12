// Parquet-backed market data reader. Compiled in only when build.sh detects
// Apache Arrow/Parquet on the system and defines AZBT_PARQUET - otherwise
// this whole file is a no-op, so existing CSV-only builds/collaborators
// don't need Arrow installed at all. See MarketData's constructor in
// marketData.h for the fallback error when a .parquet path is used without
// this support compiled in.
#pragma once

#ifdef AZBT_PARQUET

#include "dataConfig.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>

namespace mdDetail {

inline long long tsUnitToNanoMultiplier(arrow::TimeUnit::type unit) {
    switch (unit) {
        case arrow::TimeUnit::SECOND: return 1000000000LL;
        case arrow::TimeUnit::MILLI:  return 1000000LL;
        case arrow::TimeUnit::MICRO:  return 1000LL;
        case arrow::TimeUnit::NANO:   return 1LL;
    }
    return 1LL;
}

} // namespace mdDetail

/// @brief streaming reader over a Parquet file, row-group-at-a-time, mirroring
/// _MarketData's interface (skipLine/byteOffset/seekTo/nextTick/nextClose) so
/// MarketData can dispatch to either backend by file extension.
///
/// The Parquet file is expected to mirror the CSV column layout 1:1 (same
/// column indices as dataConfig.h), just typed instead of all-text.
/// timestamp/price come back through Tick as
/// string_views into a per-instance buffer (formatted on demand from the
/// typed Arrow values), so every existing consumer (tsToEpochSeconds,
/// std::from_chars on price) keeps working unmodified. Those views are only
/// valid until the next call on this instance (narrower than the mmap'd CSV
/// path's whole-lifetime guarantee) - in practice nothing holds a Tick across
/// calls, every call site converts price/timestamp to a numeric type immediately.
class _ParquetMarketData {
private:
    std::unique_ptr<parquet::arrow::FileReader> _reader;
    std::vector<int64_t> _rowGroupOffsets; // cumulative row counts, size == numRowGroups+1
    int64_t _totalRows = 0;

    std::vector<int> _neededCols;          // sorted, deduped parquet column indices to fetch
    int _tsPos = -1, _pxPos = -1, _szPos = -1, _symPos = -1, _aggPos = -1; // position within _neededCols
    long long _tsUnitMul = 1;              // multiplier from the timestamp column's unit to nanoseconds

    int _curGroup = -1;
    std::shared_ptr<arrow::Table> _table;  // keeps the current row group's arrays alive
    std::shared_ptr<arrow::TimestampArray> _tsArr;
    std::shared_ptr<arrow::DoubleArray> _pxArr;
    std::shared_ptr<arrow::Int64Array> _szArr;
    std::shared_ptr<arrow::Array> _symArr;
    std::shared_ptr<arrow::Array> _aggArr;
    bool _symLarge = false, _aggLarge = false;
    int64_t _rowInGroup = 0;

    int64_t _absoluteRow = 0; // physical row index across the whole file

    // contract roll state, mirrors _MarketData::_nextMatchingLine
    std::string _activeSymbol;
    int _missCount = 0;
    static constexpr int _rollThreshold = 200;

    char _tsBuf[40];
    char _pxBuf[32];

    void loadRowGroup(int g) {
        auto tableResult = _reader->ReadRowGroup(g, _neededCols);
        if (!tableResult.ok())
            throw std::runtime_error("ParquetMarketData: failed to read row group "
                + std::to_string(g) + ": " + tableResult.status().ToString());
        auto combined = (*tableResult)->CombineChunks();
        if (!combined.ok())
            throw std::runtime_error("ParquetMarketData: CombineChunks failed: " + combined.status().ToString());
        _table = *combined;
        _curGroup = g;

        _tsArr  = std::static_pointer_cast<arrow::TimestampArray>(_table->column(_tsPos)->chunk(0));
        _pxArr  = std::static_pointer_cast<arrow::DoubleArray>(_table->column(_pxPos)->chunk(0));
        _szArr  = std::static_pointer_cast<arrow::Int64Array>(_table->column(_szPos)->chunk(0));
        _symArr = _symPos >= 0 ? _table->column(_symPos)->chunk(0) : nullptr;
        _aggArr = _aggPos >= 0 ? _table->column(_aggPos)->chunk(0) : nullptr;
    }

    void ensureRowLoaded(int64_t rowIdx) {
        if (_curGroup >= 0 && rowIdx >= _rowGroupOffsets[static_cast<std::size_t>(_curGroup)]
            && rowIdx < _rowGroupOffsets[static_cast<std::size_t>(_curGroup) + 1]) {
            _rowInGroup = rowIdx - _rowGroupOffsets[static_cast<std::size_t>(_curGroup)];
            return;
        }
        int g = static_cast<int>(std::upper_bound(_rowGroupOffsets.begin(), _rowGroupOffsets.end(), rowIdx)
                                  - _rowGroupOffsets.begin()) - 1;
        loadRowGroup(g);
        _rowInGroup = rowIdx - _rowGroupOffsets[static_cast<std::size_t>(_curGroup)];
    }

    std::string_view curSymbol() const {
        return _symLarge ? std::static_pointer_cast<arrow::LargeStringArray>(_symArr)->GetView(_rowInGroup)
                         : std::static_pointer_cast<arrow::StringArray>(_symArr)->GetView(_rowInGroup);
    }
    std::string_view curSide() const {
        return _aggLarge ? std::static_pointer_cast<arrow::LargeStringArray>(_aggArr)->GetView(_rowInGroup)
                         : std::static_pointer_cast<arrow::StringArray>(_aggArr)->GetView(_rowInGroup);
    }
    double  curPrice()   const { return _pxArr->Value(_rowInGroup); }
    int64_t curSizeRaw() const { return _szArr->Value(_rowInGroup); }
    long long curTsNanos() const { return static_cast<long long>(_tsArr->Value(_rowInGroup)) * _tsUnitMul; }

    // advance past rows that don't match the configured symbol filter, mirrors
    // _MarketData::_nextMatchingLine but over Parquet rows; leaves _absoluteRow
    // pointing AT the next matching row (not yet consumed) on success
    bool advanceToNextMatch() {
        if (kCSVMapping.symbolCol < 0) return _absoluteRow < _totalRows;

        std::string_view root(kCSVMapping.symbol);
        while (_absoluteRow < _totalRows) {
            ensureRowLoaded(_absoluteRow);
            std::string_view sym = curSymbol();

            if (!kCSVMapping.symbolRoll) {
                if (sym == root) return true;
                ++_absoluteRow;
                continue;
            }

            if (sym.size() < root.size() || sym.substr(0, root.size()) != root) {
                ++_absoluteRow;
                continue;
            }

            if (_activeSymbol.empty()) { _activeSymbol = std::string(sym); _missCount = 0; return true; }
            if (sym == std::string_view(_activeSymbol)) { _missCount = 0; return true; }

            _missCount++;
            if (_missCount >= _rollThreshold) { _activeSymbol = std::string(sym); _missCount = 0; return true; }
            ++_absoluteRow;
        }
        return false;
    }

public:
    explicit _ParquetMarketData(const std::string& path) {
        auto fileResult = arrow::io::ReadableFile::Open(path);
        if (!fileResult.ok())
            throw std::runtime_error("ParquetMarketData: open failed: " + path + " (" + fileResult.status().ToString() + ")");

        auto readerResult = parquet::arrow::OpenFile(*fileResult, arrow::default_memory_pool());
        if (!readerResult.ok())
            throw std::runtime_error("ParquetMarketData: failed to open " + path + ": " + readerResult.status().ToString());
        _reader = std::move(*readerResult);

        std::shared_ptr<arrow::Schema> schema;
        auto status = _reader->GetSchema(&schema);
        if (!status.ok())
            throw std::runtime_error("ParquetMarketData: failed to read schema of " + path + ": " + status.ToString());

        auto requireField = [&](int col, const char* role) -> std::shared_ptr<arrow::Field> {
            if (col < 0 || col >= schema->num_fields())
                throw std::runtime_error("ParquetMarketData: configured " + std::string(role) + " column index "
                    + std::to_string(col) + " is out of range for " + path);
            return schema->field(col);
        };

        auto tsField = requireField(kCSVMapping.timestampCol, "timestamp");
        if (tsField->type()->id() != arrow::Type::TIMESTAMP)
            throw std::runtime_error("ParquetMarketData: timestampCol is not a timestamp column in " + path
                + " (got " + tsField->type()->ToString() + ")");
        _tsUnitMul = mdDetail::tsUnitToNanoMultiplier(
            std::static_pointer_cast<arrow::TimestampType>(tsField->type())->unit());

        auto pxField = requireField(kCSVMapping.priceCol, "price");
        if (pxField->type()->id() != arrow::Type::DOUBLE)
            throw std::runtime_error("ParquetMarketData: priceCol must be a double column in " + path
                + " (got " + pxField->type()->ToString() + ")");

        auto szField = requireField(kCSVMapping.sizeCol, "size");
        if (szField->type()->id() != arrow::Type::INT64)
            throw std::runtime_error("ParquetMarketData: sizeCol must be an int64 column in " + path
                + " (got " + szField->type()->ToString() + ")");

        if (kCSVMapping.symbolCol >= 0) {
            auto symField = requireField(kCSVMapping.symbolCol, "symbol");
            auto sid = symField->type()->id();
            if (sid != arrow::Type::STRING && sid != arrow::Type::LARGE_STRING)
                throw std::runtime_error("ParquetMarketData: symbolCol must be a string column in " + path
                    + " (got " + symField->type()->ToString() + ")");
            _symLarge = (sid == arrow::Type::LARGE_STRING);
        }
        if (kCSVMapping.aggressor >= 0) {
            auto aggField = requireField(kCSVMapping.aggressor, "aggressor");
            auto aid = aggField->type()->id();
            if (aid != arrow::Type::STRING && aid != arrow::Type::LARGE_STRING)
                throw std::runtime_error("ParquetMarketData: aggressor column must be a string column in " + path
                    + " (got " + aggField->type()->ToString() + ")");
            _aggLarge = (aid == arrow::Type::LARGE_STRING);
        }

        std::vector<int> cols = { kCSVMapping.timestampCol, kCSVMapping.priceCol, kCSVMapping.sizeCol };
        if (kCSVMapping.symbolCol >= 0) cols.push_back(kCSVMapping.symbolCol);
        if (kCSVMapping.aggressor >= 0) cols.push_back(kCSVMapping.aggressor);
        std::sort(cols.begin(), cols.end());
        cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
        _neededCols = cols;

        auto posOf = [&](int col) {
            return static_cast<int>(std::lower_bound(_neededCols.begin(), _neededCols.end(), col) - _neededCols.begin());
        };
        _tsPos  = posOf(kCSVMapping.timestampCol);
        _pxPos  = posOf(kCSVMapping.priceCol);
        _szPos  = posOf(kCSVMapping.sizeCol);
        _symPos = kCSVMapping.symbolCol >= 0 ? posOf(kCSVMapping.symbolCol) : -1;
        _aggPos = kCSVMapping.aggressor >= 0 ? posOf(kCSVMapping.aggressor) : -1;

        int numRowGroups = _reader->parquet_reader()->metadata()->num_row_groups();
        _rowGroupOffsets.assign(static_cast<std::size_t>(numRowGroups) + 1, 0);
        for (int i = 0; i < numRowGroups; i++) {
            int64_t rows = _reader->parquet_reader()->metadata()->RowGroup(i)->num_rows();
            _rowGroupOffsets[static_cast<std::size_t>(i) + 1] = _rowGroupOffsets[static_cast<std::size_t>(i)] + rows;
        }
        _totalRows = _rowGroupOffsets.empty() ? 0 : _rowGroupOffsets.back();
    }

    bool skipLine() {
        if (_absoluteRow >= _totalRows) return false;
        ++_absoluteRow;
        return true;
    }

    std::size_t byteOffset() const { return static_cast<std::size_t>(_absoluteRow); }
    void seekTo(std::size_t off)   { _absoluteRow = static_cast<int64_t>(off); }

    std::optional<Tick> nextTick() {
        if (!advanceToNextMatch()) return std::nullopt;

        Tick t;
        int n = mdDetail::formatIsoTimestamp(_tsBuf, sizeof(_tsBuf), curTsNanos());
        t.timestamp = std::string_view(_tsBuf, static_cast<std::size_t>(n));

        auto [ptr, ec] = std::to_chars(_pxBuf, _pxBuf + sizeof(_pxBuf), curPrice());
        t.price = std::string_view(_pxBuf, static_cast<std::size_t>(ptr - _pxBuf));

        t.size = static_cast<float>(curSizeRaw());

        if (_aggPos >= 0) {
            std::string_view sideView = curSide();
            if (sideView == kCSVMapping.buySideAggressorAlias) t.side = kCSVMapping.buySideAggressorAlias;
            else if (sideView == kCSVMapping.sellSideAggressorAlias) t.side = kCSVMapping.sellSideAggressorAlias;
        }

        ++_absoluteRow;
        return t;
    }

    std::optional<Tick> nextClose(int seconds) {
        if (!advanceToNextMatch()) return std::nullopt;

        int firstLen = mdDetail::formatIsoTimestamp(_tsBuf, sizeof(_tsBuf), curTsNanos());
        std::string firstTs(_tsBuf, static_cast<std::size_t>(firstLen));
        std::string targetOwned = mdDetail::endTimestamp(firstTs, seconds);
        std::string_view target(targetOwned);

        float barVolume = static_cast<float>(curSizeRaw());
        double lastPx = curPrice();
        std::string lastTs = firstTs;
        ++_absoluteRow;

        while (std::string_view(lastTs) < target) {
            if (!advanceToNextMatch()) break;
            int len = mdDetail::formatIsoTimestamp(_tsBuf, sizeof(_tsBuf), curTsNanos());
            lastTs.assign(_tsBuf, static_cast<std::size_t>(len));
            lastPx = curPrice();
            barVolume += static_cast<float>(curSizeRaw());
            ++_absoluteRow;
        }

        Tick t;
        std::memcpy(_tsBuf, lastTs.data(), lastTs.size());
        t.timestamp = std::string_view(_tsBuf, lastTs.size());
        auto [ptr, ec] = std::to_chars(_pxBuf, _pxBuf + sizeof(_pxBuf), lastPx);
        t.price = std::string_view(_pxBuf, static_cast<std::size_t>(ptr - _pxBuf));
        t.size = barVolume;
        return t;
    }
};

#endif // AZBT_PARQUET
