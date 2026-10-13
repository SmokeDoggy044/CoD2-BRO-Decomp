#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace loader {

// Function names exported from Ghidra (tools/ghidra/ExportSymbols.java), used to
// make logs and crash reports readable. Format: one "hexaddress name" per line.
class SymbolTable {
public:
    bool load(const std::filesystem::path& path);
    size_t size() const { return entries_.size(); }

    // "name+0x12", or the bare hex address if nothing precedes it.
    std::string describe(uint32_t address) const;

private:
    struct Entry {
        uint32_t address;
        std::string name;
    };
    std::vector<Entry> entries_;
};

}  // namespace loader
