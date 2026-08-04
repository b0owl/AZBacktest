#pragma once
#include <string>
#include <vector>
#include <type_traits>

namespace seriesPool {

/// @brief rgba color, all -1 means "let the renderer pick"
struct RGBA {
    float r=-1, g=-1, b=-1, a=-1;
    bool isSet() const { return r >= 0; }
};

/// @brief one entry in the global series pool
/// data is column-major: data[col][row], a simple 1D series has one column
struct NamedSeries {
    std::string name;
    std::vector<std::vector<float>> data;      ///< data[column][row]
    std::vector<std::string> columnNames;       ///< optional per-column names
    int type;                                   ///< 0 = lineplot, 1 = barplot
    RGBA color;                                 ///< all columns share this color, -1 = auto

    int cols() const { return (int)data.size(); }       ///< number of columns (1 for a simple series)
    int rows() const { return data.empty() ? 0 : (int)data[0].size(); } ///< number of data points per column

    /// @brief get display name for a column, falls back to index if unnamed
    std::string colName(int col) const {
        if (col < (int)columnNames.size() && !columnNames[col].empty())
            return columnNames[col];
        return std::to_string(col);
    }
};

/// @brief process-wide pool of series, everything that's been added lives here
inline std::vector<NamedSeries> pool;

/// @brief wipe the pool so you can start fresh (e.g. between backtests)
inline void clear() { pool.clear(); }

/// @brief add a 1D series (single column) to the pool
/// @param name  display name for the series
/// @param values the data, any arithmetic type gets converted to float
/// @param type  0 = line, 1 = bar
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void addSeries(std::string name, std::vector<T> values, int type = 0, RGBA color = {}) {
    std::vector<float> col(values.begin(), values.end());
    pool.push_back({std::move(name), {std::move(col)}, {}, type, color});
}

/// @brief add a 2D series (multiple columns) to the pool
/// @param name     display name for the series
/// @param values   vector of columns, each column is a vector of values
/// @param colNames optional names for each column (shows up in explorer + plot legend)
/// @param type     0 = line, 1 = bar
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void addSeries(std::string name, std::vector<std::vector<T>> values,
               std::vector<std::string> colNames = {}, int type = 0, RGBA color = {}) {
    std::vector<std::vector<float>> cols;
    cols.reserve(values.size());
    for (auto& v : values) {
        cols.emplace_back(v.begin(), v.end());
    }
    pool.push_back({std::move(name), std::move(cols), std::move(colNames), type, color});
}

/// @brief batch init for the simple case where each inner vector is its own
/// 1-column series. clears the pool first so this is a full reset
/// @param data  one series per outer vector
/// @param names display name per series (auto-named if shorter than data)
/// @param types 0 = line, 1 = bar per series (defaults to 0 if shorter)
inline void initSeriesPool(std::vector<std::vector<float>> data,
                           std::vector<std::string> names,
                           std::vector<int> types) {
    pool.clear();
    pool.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        std::string n = i < names.size() ? names[i] : "series " + std::to_string(i);
        int t = i < types.size() ? types[i] : 0;
        pool.push_back({std::move(n), {std::move(data[i])}, {}, t, {}});
    }
}

} // namespace seriesPool
