#include "Database.h"
#include "Types.h"
#include <cmath>
#include <sqlite3.h>

namespace sba {

Database::Database() = default;
Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    close();
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        m_error = m_db ? sqlite3_errmsg(m_db) : "out of memory";
        close();
        return false;
    }
    sqlite3_busy_timeout(m_db, 3000);
    return exec("PRAGMA journal_mode=WAL;") && exec("PRAGMA foreign_keys=ON;") && exec("PRAGMA synchronous=NORMAL;");
}

void Database::close() {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool Database::exec(const std::string& sql) {
    if (!m_db) { m_error = "database not open"; return false; }
    char* err = nullptr;
    const int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) { m_error = err ? err : "unknown SQL error"; sqlite3_free(err); return false; }
    return true;
}

std::int64_t Database::lastInsertId() const { return m_db ? sqlite3_last_insert_rowid(m_db) : 0; }

int Database::userVersion() {
    Statement s(*this, "PRAGMA user_version;");
    return s.step() ? static_cast<int>(s.integer(0)) : 0;
}
void Database::setUserVersion(int v) { exec("PRAGMA user_version=" + std::to_string(v) + ";"); }

Statement::Statement(Database& db, const std::string& sql) : m_db(db) {
    if (sqlite3_prepare_v2(db.m_db, sql.c_str(), -1, &m_stmt, nullptr) != SQLITE_OK) {
        db.m_error = sqlite3_errmsg(db.m_db);
        m_stmt = nullptr;
    }
}
Statement::~Statement() { if (m_stmt) sqlite3_finalize(m_stmt); }

Statement& Statement::bind(int i, const std::string& v) { if (m_stmt) sqlite3_bind_text(m_stmt, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT); return *this; }
Statement& Statement::bind(int i, double v) {
    if (m_stmt) { if (std::isnan(v)) sqlite3_bind_null(m_stmt, i); else sqlite3_bind_double(m_stmt, i, v); }
    return *this;
}
Statement& Statement::bind(int i, std::int64_t v) { if (m_stmt) sqlite3_bind_int64(m_stmt, i, v); return *this; }
Statement& Statement::bindNull(int i) { if (m_stmt) sqlite3_bind_null(m_stmt, i); return *this; }

bool Statement::step() { return m_stmt && sqlite3_step(m_stmt) == SQLITE_ROW; }
bool Statement::run() {
    if (!m_stmt) return false;
    const int rc = sqlite3_step(m_stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) { m_db.m_error = sqlite3_errmsg(m_db.m_db); return false; }
    return true;
}
std::string Statement::text(int c) const {
    const unsigned char* t = sqlite3_column_text(m_stmt, c);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}
double Statement::real(int c) const { return sqlite3_column_type(m_stmt, c) == SQLITE_NULL ? kNaN : sqlite3_column_double(m_stmt, c); }
std::int64_t Statement::integer(int c) const { return sqlite3_column_int64(m_stmt, c); }
bool Statement::isNull(int c) const { return sqlite3_column_type(m_stmt, c) == SQLITE_NULL; }

}  // namespace sba
