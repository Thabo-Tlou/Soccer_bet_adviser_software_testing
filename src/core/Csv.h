#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace sba {

struct CsvTable {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    std::unordered_map<std::string, size_t> index;

    int col(const std::string& name) const {
        auto it = index.find(name);
        return it == index.end() ? -1 : static_cast<int>(it->second);
    }
};

// Splits one CSV line (RFC 4180 quotes supported, trailing \r removed).
std::vector<std::string> splitCsvLine(const std::string& line);

// Loads a CSV file. Returns false and fills `error` when unreadable/empty.
bool loadCsv(const std::string& path, CsvTable& out, std::string& error);

// Parses a number; empty / non-numeric text becomes NaN.
double parseDouble(const std::string& s);

}  // namespace sba
