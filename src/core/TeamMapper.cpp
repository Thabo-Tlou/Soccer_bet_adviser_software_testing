#include "TeamMapper.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace sba {

std::string normaliseTeamName(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
        else if (c == '&') out += " and ";
        else out += ' ';
    }
    // drop generic words
    std::string res, word;
    auto flush = [&]() {
        if (!word.empty() && word != "fc" && word != "afc" && word != "cf" && word != "and") {
            if (!res.empty()) res += ' ';
            res += word;
        }
        word.clear();
    };
    for (char c : out) { if (c == ' ') flush(); else word += c; }
    flush();
    return res;
}

std::string mapTeamName(const std::string& provider, const std::vector<std::string>& teams) {
    static const std::map<std::string, std::string> alias = {
        {"manchester city", "Man City"}, {"manchester united", "Man United"},
        {"nottingham forest", "Nott'm Forest"}, {"wolverhampton wanderers", "Wolves"},
        {"west bromwich albion", "West Brom"}, {"spurs", "Tottenham"},
        {"brighton hove albion", "Brighton"}, {"sheffield utd", "Sheffield United"},
        {"man utd", "Man United"}, {"newcastle utd", "Newcastle"}, {"nottm forest", "Nott'm Forest"},
        {"leeds utd", "Leeds"}, {"wolves", "Wolves"},
    };
    const std::string n = normaliseTeamName(provider);
    if (n.empty()) return "";
    auto a = alias.find(n);
    if (a != alias.end() && std::find(teams.begin(), teams.end(), a->second) != teams.end()) return a->second;
    for (const auto& t : teams) if (normaliseTeamName(t) == n) return t;
    // prefix match, longest dataset name wins ("newcastle united" -> "Newcastle")
    std::string best; size_t bestLen = 0;
    for (const auto& t : teams) {
        const std::string tn = normaliseTeamName(t);
        if (tn.size() >= 4 && n.compare(0, tn.size(), tn) == 0 && (n.size() == tn.size() || n[tn.size()] == ' ') && tn.size() > bestLen) {
            best = t; bestLen = tn.size();
        }
    }
    return best;
}

}  // namespace sba
