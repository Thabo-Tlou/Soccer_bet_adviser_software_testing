#include "TfIdfIndex.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace sba {
namespace {

constexpr int kChunkLines = 40, kChunkStep = 30;
constexpr std::uintmax_t kMaxFileBytes = 400 * 1024;

const std::set<std::string> kStop = {"the", "a", "an", "is", "are", "was", "of", "to", "in", "on", "for", "and", "or", "where",
                                     "what", "how", "does", "do", "it", "this", "that", "with", "be", "by", "at", "from", "which",
                                     "int", "const", "return", "void", "auto", "std", "if", "else"};
const std::set<std::string> kSkipDirs = {"build", ".git", "venv", ".venv", "node_modules", ".vs", ".idea", "out", "bin", "obj",
                                         "__pycache__", "fixtures", "cmake-build-debug", "cmake-build-release", "_deps", "Testing"};
const std::set<std::string> kExt = {".cpp", ".h", ".hpp", ".c", ".ui", ".txt", ".md", ".cmake", ".json", ".qss", ".py", ".ps1", ".qrc"};

bool isSecretish(const std::string& name) {
    std::string n = name;
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return n.find("secret") != std::string::npos || n.find(".env") != std::string::npos || n.find("apikey") != std::string::npos ||
           n.find("api_key") != std::string::npos || n.find("credential") != std::string::npos || n.find("local_config") != std::string::npos ||
           n.find("poisson_model") != std::string::npos || (n.size() > 4 && n.substr(n.size() - 4) == ".key");
}

bool includeFile(const fs::path& p, std::uintmax_t size) {
    if (size == 0 || size > kMaxFileBytes) return false;
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!kExt.count(ext)) return false;
    return !isSecretish(p.filename().string());
}

std::string stem(std::string w) {
    static const char* suf[] = {"izing", "ising", "ized", "ised", "izes", "ises", "ize", "ise", "ations", "ation", "ings", "ing", "ers", "er", "ed", "es", "s"};
    for (const char* s : suf) {
        const std::string sf = s;
        if (w.size() > sf.size() + 3 && w.compare(w.size() - sf.size(), sf.size(), sf) == 0) { w.erase(w.size() - sf.size()); break; }
    }
    return w;
}

void addToken(std::vector<std::string>& out, const std::string& w) {
    if (w.size() < 2 || w.size() > 40) return;
    if (std::all_of(w.begin(), w.end(), [](unsigned char c) { return std::isdigit(c); })) return;
    if (kStop.count(w)) return;
    out.push_back(stem(w));
}

std::string maskSecrets(const std::string& line) {
    std::string out; size_t i = 0;
    while (i < line.size()) {
        if (std::isalnum(static_cast<unsigned char>(line[i]))) {
            size_t j = i; bool dig = false, alp = false;
            while (j < line.size() && std::isalnum(static_cast<unsigned char>(line[j]))) { dig |= std::isdigit(static_cast<unsigned char>(line[j])) != 0; alp |= std::isalpha(static_cast<unsigned char>(line[j])) != 0; ++j; }
            if (j - i >= 28 && dig && alp) out += "****"; else out.append(line, i, j - i);
            i = j;
        } else out += line[i++];
    }
    return out;
}

}  // namespace

std::vector<std::string> TfIdfIndex::tokenize(const std::string& text) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size()) {
        if (!(std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) { ++i; continue; }
        size_t j = i;
        while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j])) || text[j] == '_')) ++j;
        const std::string ident = text.substr(i, j - i);
        i = j;
        if (ident.size() >= 28) {   // long mixed tokens look like keys: ignore
            bool d = false, a = false;
            for (unsigned char c : ident) { d |= std::isdigit(c) != 0; a |= std::isalpha(c) != 0; }
            if (d && a) continue;
        }
        std::string lowerFull;
        for (char c : ident) if (c != '_') lowerFull += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        // split into words: underscores and camelCase / ACRONYMWord boundaries
        std::vector<std::string> words; std::string cur;
        for (size_t k = 0; k < ident.size(); ++k) {
            const unsigned char c = ident[k];
            if (c == '_') { if (!cur.empty()) words.push_back(cur); cur.clear(); continue; }
            const bool upper = std::isupper(c) != 0;
            const bool prevLower = k > 0 && (std::islower(static_cast<unsigned char>(ident[k - 1])) || std::isdigit(static_cast<unsigned char>(ident[k - 1])));
            const bool nextLower = k + 1 < ident.size() && std::islower(static_cast<unsigned char>(ident[k + 1]));
            const bool prevUpper = k > 0 && std::isupper(static_cast<unsigned char>(ident[k - 1]));
            if (upper && (prevLower || (prevUpper && nextLower)) && !cur.empty()) { words.push_back(cur); cur.clear(); }
            cur += static_cast<char>(std::tolower(c));
        }
        if (!cur.empty()) words.push_back(cur);
        for (const auto& w : words) addToken(out, w);
        if (words.size() > 1) addToken(out, lowerFull);   // keep the whole identifier too ("onnxinference")
    }
    return out;
}

std::uint64_t TfIdfIndex::signatureOf(const std::string& root) {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&](const std::string& s) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } };
    std::error_code ec;
    std::vector<std::string> parts;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) { if (kSkipDirs.count(it->path().filename().string())) it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const auto size = it->file_size(ec);
        if (!includeFile(it->path(), size)) continue;
        parts.push_back(it->path().string() + "|" + std::to_string(size) + "|" + std::to_string(it->last_write_time(ec).time_since_epoch().count()));
    }
    std::sort(parts.begin(), parts.end());
    for (const auto& p : parts) mix(p);
    return h;
}

size_t TfIdfIndex::build(const std::string& root) {
    m_root = root; m_chunks.clear(); m_idf.clear(); m_postings.clear(); m_files = 0;
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) { if (kSkipDirs.count(it->path().filename().string())) it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec)) continue;
        if (includeFile(it->path(), it->file_size(ec))) files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& p : files) {
        std::ifstream in(p, std::ios::binary);
        if (!in) continue;
        std::vector<std::string> lines; std::string line;
        while (std::getline(in, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); lines.push_back(maskSecrets(line)); }
        if (lines.empty()) continue;
        ++m_files;
        const std::string rel = fs::relative(p, root, ec).generic_string();
        for (size_t s = 0; s < lines.size(); s += kChunkStep) {
            const size_t e = std::min(lines.size(), s + kChunkLines);
            Chunk c; c.file = rel; c.start = static_cast<int>(s) + 1; c.end = static_cast<int>(e);
            std::ostringstream ss;
            for (size_t k = s; k < e; ++k) ss << lines[k] << '\n';
            c.text = ss.str();
            m_chunks.push_back(std::move(c));
            if (e == lines.size()) break;
        }
    }

    // document frequencies
    const size_t N = m_chunks.size();
    std::vector<std::unordered_map<std::string, int>> tf(N);
    std::unordered_map<std::string, int> df;
    for (size_t i = 0; i < N; ++i) {
        for (const auto& t : tokenize(m_chunks[i].text)) tf[i][t]++;
        for (const auto& t : tokenize(m_chunks[i].file)) tf[i][t] += 3;   // file/path words weigh more
        for (const auto& kv : tf[i]) df[kv.first]++;
    }
    for (const auto& kv : df) m_idf[kv.first] = std::log((1.0 + N) / (1.0 + kv.second)) + 1.0;
    for (size_t i = 0; i < N; ++i) {
        double norm = 0;
        std::vector<std::pair<std::string, double>> w;
        for (const auto& kv : tf[i]) { const double x = (1.0 + std::log(static_cast<double>(kv.second))) * m_idf[kv.first]; w.push_back({kv.first, x}); norm += x * x; }
        norm = std::sqrt(norm);
        if (norm == 0) continue;
        for (const auto& p : w) m_postings[p.first].push_back({static_cast<uint32_t>(i), static_cast<float>(p.second / norm)});
    }
    m_signature = signatureOf(root);
    return N;
}

bool TfIdfIndex::refreshIfChanged(const std::string& root) {
    if (root == m_root && !m_chunks.empty() && signatureOf(root) == m_signature) return false;
    build(root);
    return true;
}

std::vector<SearchHit> TfIdfIndex::search(const std::string& query, size_t maxResults) const {
    std::vector<SearchHit> hits;
    if (m_chunks.empty()) return hits;
    static const std::map<std::string, std::vector<std::string>> synonyms = {
        {"initial", {"load", "init", "create", "setup", "session"}}, {"init", {"load", "create", "initialise"}},
        {"start", {"main", "run", "exec"}}, {"setting", {"config", "option"}}, {"store", {"database", "sqlite", "save"}},
    };
    std::unordered_map<std::string, int> qtf;
    for (const auto& t : tokenize(query)) {
        qtf[t]++;
    }
    std::vector<std::pair<std::string, double>> q;
    double qn = 0;
    for (const auto& kv : qtf) {
        auto it = m_idf.find(kv.first);
        if (it == m_idf.end()) continue;
        const double w = (1.0 + std::log(static_cast<double>(std::max(1, kv.second)))) * it->second;
        q.push_back({kv.first, w}); qn += w * w;
    }
    // synonym expansion at reduced weight
    for (const auto& t : tokenize(query)) {
        auto s = synonyms.find(t);
        if (s == synonyms.end()) continue;
        for (const auto& x : s->second) {
            auto it = m_idf.find(stem(x));
            if (it == m_idf.end()) continue;
            const double w = 0.35 * it->second;
            q.push_back({stem(x), w}); qn += w * w;
        }
    }
    if (q.empty()) return hits;
    qn = std::sqrt(qn);
    std::unordered_map<uint32_t, double> score;
    for (const auto& term : q) {
        auto p = m_postings.find(term.first);
        if (p == m_postings.end()) continue;
        for (const auto& d : p->second) score[d.first] += (term.second / qn) * d.second;
    }
    // Coverage boost: chunks that contain every query concept (a word or one of its synonyms) rank first.
    {
        std::vector<std::set<std::string>> groups;
        for (const auto& t : tokenize(query)) {
            std::set<std::string> g{t};
            auto s = synonyms.find(t);
            if (s != synonyms.end()) for (const auto& x : s->second) g.insert(stem(x));
            groups.push_back(g);
        }
        std::unordered_map<uint32_t, int> covered;
        for (const auto& g : groups) {
            std::set<uint32_t> seen;
            for (const auto& term : g) {
                auto p = m_postings.find(term);
                if (p != m_postings.end()) for (const auto& d : p->second) seen.insert(d.first);
            }
            for (uint32_t c : seen) covered[c]++;
        }
        if (!groups.empty())
            for (auto& kv : score) {
                const double cov = static_cast<double>(covered[kv.first]) / groups.size();
                kv.second *= 0.4 + 0.6 * cov * cov;
            }
    }
    std::vector<std::pair<uint32_t, double>> ranked(score.begin(), score.end());
    std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.second > b.second || (a.second == b.second && a.first < b.first); });

    std::set<std::string> qterms; for (const auto& t : q) qterms.insert(t.first);
    for (const auto& r : ranked) {
        if (hits.size() >= maxResults) break;
        const Chunk& c = m_chunks[r.first];
        // skip chunks that heavily overlap a better hit from the same file
        bool dup = false;
        for (const auto& h : hits) if (h.file == c.file && std::abs(h.startLine - c.start) < 20) { dup = true; break; }
        if (dup) continue;
        SearchHit h; h.file = c.file; h.startLine = c.start; h.endLine = c.end; h.score = r.second; h.text = c.text;
        std::istringstream ss(c.text); std::string ln; int idx = 0;
        std::vector<std::pair<int, int>> scored;               // (matching distinct terms, line index)
        std::vector<std::string> lines;
        while (std::getline(ss, ln)) {
            std::set<std::string> found;
            for (const auto& t : tokenize(ln)) if (qterms.count(t)) found.insert(t);
            scored.push_back({static_cast<int>(found.size()), idx++});
            lines.push_back(ln);
        }
        std::stable_sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });
        std::vector<int> pick;
        for (const auto& s : scored) if (s.first > 0 && pick.size() < 3) pick.push_back(s.second);
        std::sort(pick.begin(), pick.end());
        for (int i : pick) h.snippet += std::to_string(c.start + i) + ": " + lines[i] + "\n";
        if (h.snippet.empty()) h.snippet = std::to_string(c.start) + ": " + c.text.substr(0, c.text.find('\n')) + "\n";
        hits.push_back(std::move(h));
    }
    return hits;
}

}  // namespace sba
