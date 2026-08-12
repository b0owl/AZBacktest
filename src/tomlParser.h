// minimal toml parser, only handles flat key=value pairs with one level
// of [section] nesting. enough for config.toml, not a full toml impl
#pragma once

#include <fstream>
#include <map>
#include <string>
#include <stdexcept>

namespace toml {

using Table = std::map<std::string, std::map<std::string, std::string>>;

inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

inline Table parse(const std::string& path) {
    Table table;
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("config not found: " + path);

    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        // strip inline comments outside of quoted strings
        if (!val.empty() && val.front() != '"') {
            auto hash = val.find('#');
            if (hash != std::string::npos)
                val = trim(val.substr(0, hash));
        }

        val = unquote(val);
        table[section][key] = val;
    }
    return table;
}

inline int getInt(const Table& t, const std::string& section,
                  const std::string& key, int def = 0) {
    auto sit = t.find(section);
    if (sit == t.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    try { return std::stoi(kit->second); } catch (...) { return def; }
}

inline float getFloat(const Table& t, const std::string& section,
                      const std::string& key, float def = 0.f) {
    auto sit = t.find(section);
    if (sit == t.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    try { return std::stof(kit->second); } catch (...) { return def; }
}

inline bool getBool(const Table& t, const std::string& section,
                    const std::string& key, bool def = false) {
    auto sit = t.find(section);
    if (sit == t.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    return kit->second == "true" || kit->second == "1";
}

inline std::string getString(const Table& t, const std::string& section,
                             const std::string& key, const std::string& def = "") {
    auto sit = t.find(section);
    if (sit == t.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    return kit->second;
}

} // namespace toml
