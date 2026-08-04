#pragma once
#include <string>
#include <vector>

namespace statPool {

struct Stat {
    std::string name;
    float value;
};

/// @brief process-wide pool of logged stats
inline std::vector<Stat> pool;

/// @brief log a stat by name, overwrites if it already exists
inline void addStat(std::string name, float value) {
    for (auto& s : pool) {
        if (s.name == name) { s.value = value; return; }
    }
    pool.push_back({std::move(name), value});
}

inline void clear() { pool.clear(); }

} // namespace statPool
