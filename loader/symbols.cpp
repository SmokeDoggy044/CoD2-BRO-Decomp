#include "symbols.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace loader {

bool SymbolTable::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string address, name;
        if (!(fields >> address >> name) || address[0] == '#') continue;
        entries_.push_back({static_cast<uint32_t>(std::stoul(address, nullptr, 16)), name});
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.address < b.address; });
    return true;
}

std::string SymbolTable::describe(uint32_t address) const {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%08x", address);
    auto it = std::upper_bound(entries_.begin(), entries_.end(), address,
                               [](uint32_t a, const Entry& e) { return a < e.address; });
    if (it == entries_.begin()) return buffer;
    --it;
    std::string result = std::string(buffer) + " " + it->name;
    if (address != it->address) {
        std::snprintf(buffer, sizeof(buffer), "+0x%x", address - it->address);
        result += buffer;
    }
    return result;
}

}  // namespace loader
