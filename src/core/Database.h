#pragma once
// Thin RAII wrapper over SQLite with prepared statements (no string-built SQL).
#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace sba {

class Statement;

class Database {
public:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& path);   // creates the file if needed, enables WAL + foreign keys
    void close();
    bool isOpen() const { return m_db != nullptr; }
    bool exec(const std::string& sql);    // for static DDL only - never for user data
    const std::string& lastError() const { return m_error; }
    std::int64_t lastInsertId() const;
    int userVersion();
    void setUserVersion(int v);
    sqlite3* handle() const { return m_db; }

private:
    friend class Statement;
    sqlite3* m_db = nullptr;
    std::string m_error;
};

class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool valid() const { return m_stmt != nullptr; }
    Statement& bind(int idx, const std::string& v);
    Statement& bind(int idx, double v);        // NaN is stored as NULL
    Statement& bind(int idx, std::int64_t v);
    Statement& bind(int idx, int v) { return bind(idx, static_cast<std::int64_t>(v)); }
    Statement& bindNull(int idx);
    bool step();                               // true while a row is available
    bool run();                                // step once, true on success (for INSERT/UPDATE)
    std::string text(int col) const;
    double real(int col) const;                // NULL -> NaN
    std::int64_t integer(int col) const;
    bool isNull(int col) const;

private:
    Database& m_db;
    sqlite3_stmt* m_stmt = nullptr;
};

}  // namespace sba
