#include "Csv.h"
#include "Types.h"
#include <cstdlib>
#include <fstream>

namespace sba {

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else quoted = false;
            } else cur += c;
        } else if (c == '"') quoted = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r' && c != '\n') cur += c;
    }
    out.push_back(cur);
    return out;
}

bool loadCsv(const std::string& path, CsvTable& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { error = "Cannot open file: " + path; return false; }
    out = CsvTable{};
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);  // UTF-8 BOM
            out.header = splitCsvLine(line);
            for (size_t i = 0; i < out.header.size(); ++i) out.index[out.header[i]] = i;
            first = false;
            continue;
        }
        if (line.empty() || line == "\r") continue;
        out.rows.push_back(splitCsvLine(line));
    }
    if (out.header.empty()) { error = "CSV file is empty: " + path; return false; }
    return true;
}

double parseDouble(const std::string& s) {
    if (s.empty()) return kNaN;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return kNaN;
    return v;
}

}  // namespace sba
