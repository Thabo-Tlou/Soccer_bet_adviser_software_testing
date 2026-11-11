#pragma once
// Phase 7 - native TF-IDF search over the project's own source and documentation.
// Retrieval only: it finds relevant text, it does not understand or compile code.
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace sba {

struct SearchHit {
    std::string file;        // path relative to the indexed root
    int startLine = 0, endLine = 0;
    double score = 0.0;
    std::string snippet;
    std::string text;        // whole chunk (for the preview pane)
};

class TfIdfIndex {
public:
    // Builds (or rebuilds) the index. Returns number of chunks.
    size_t build(const std::string& root);
    // Rebuilds only if a file was added, removed or changed. True when rebuilt.
    bool refreshIfChanged(const std::string& root);

    std::vector<SearchHit> search(const std::string& query, size_t maxResults = 10) const;

    size_t chunkCount() const { return m_chunks.size(); }
    size_t fileCount() const { return m_files; }
    size_t termCount() const { return m_idf.size(); }
    const std::string& root() const { return m_root; }

    static std::vector<std::string> tokenize(const std::string& text);   // exposed for tests

private:
    struct Chunk { std::string file; int start = 0, end = 0; std::string text; };
    std::string m_root;
    std::vector<Chunk> m_chunks;
    std::unordered_map<std::string, double> m_idf;
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, float>>> m_postings;  // term -> (chunk, weight)
    size_t m_files = 0;
    std::uint64_t m_signature = 0;
    static std::uint64_t signatureOf(const std::string& root);
};

}  // namespace sba
