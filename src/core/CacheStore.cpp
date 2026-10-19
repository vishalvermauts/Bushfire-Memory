#include "bushfire/CacheStore.hpp"
#include "bushfire/GeoUtils.hpp"
#include <chrono>
#include <iostream>

namespace bushfire {

CacheStore::CacheStore(const std::string& dbPath) : dbPath_(dbPath) {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open SQLite database: " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
    } else {
        initSchema();
    }
}

CacheStore::~CacheStore() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void CacheStore::initSchema() {
    const char* schemaSql = R"(
        CREATE TABLE IF NOT EXISTS http_cache (
            key TEXT PRIMARY KEY,
            payload TEXT NOT NULL,
            fetched_at_iso TEXT NOT NULL,
            expires_at_epoch INTEGER NOT NULL,
            etag TEXT,
            last_modified TEXT
        );
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at_iso TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_cache_expires ON http_cache (expires_at_epoch);
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, schemaSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite init schema error: " << (errMsg ? errMsg : "unknown") << std::endl;
        sqlite3_free(errMsg);
    }
}

bool CacheStore::set(const std::string& key, const std::string& payload, int ttlSeconds,
                     const std::string& etag, const std::string& lastModified) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    auto now = std::chrono::system_clock::now();
    long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    long expiresEpoch = nowEpoch + ttlSeconds;
    std::string fetchedAtIso = GeoUtils::getCurrentUtcIso();

    const char* sql = R"(
        INSERT OR REPLACE INTO http_cache (key, payload, fetched_at_iso, expires_at_epoch, etag, last_modified)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fetchedAtIso.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, expiresEpoch);
    sqlite3_bind_text(stmt, 5, etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, lastModified.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::optional<CacheEntry> CacheStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    const char* sql = "SELECT payload, fetched_at_iso, expires_at_epoch, etag, last_modified FROM http_cache WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        CacheEntry entry;
        entry.key = key;
        entry.payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.fetchedAtIso = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.expiresAtEpoch = sqlite3_column_int64(stmt, 2);
        
        const unsigned char* et = sqlite3_column_text(stmt, 3);
        if (et) entry.etag = reinterpret_cast<const char*>(et);
        
        const unsigned char* lm = sqlite3_column_text(stmt, 4);
        if (lm) entry.lastModified = reinterpret_cast<const char*>(lm);

        auto now = std::chrono::system_clock::now();
        long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        if (nowEpoch <= entry.expiresAtEpoch) {
            entry.freshness = FreshnessStatus::CachedFresh;
        } else {
            entry.freshness = FreshnessStatus::StaleCache;
        }

        sqlite3_finalize(stmt);
        return entry;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool CacheStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM http_cache WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool CacheStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM http_cache;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

size_t CacheStore::evictExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;

    auto now = std::chrono::system_clock::now();
    long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    const char* sql = "DELETE FROM http_cache WHERE expires_at_epoch < ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_int64(stmt, 1, nowEpoch);
    sqlite3_step(stmt);
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    return static_cast<size_t>(changes);
}

CacheStats CacheStore::getStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheStats stats;
    if (!db_) return stats;

    auto now = std::chrono::system_clock::now();
    long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    const char* sql = "SELECT expires_at_epoch, length(payload) FROM http_cache;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return stats;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        long exp = sqlite3_column_int64(stmt, 0);
        int bytes = sqlite3_column_int(stmt, 1);

        stats.totalEntries++;
        stats.totalBytes += bytes;
        if (nowEpoch <= exp) {
            stats.freshEntries++;
        } else {
            stats.staleEntries++;
        }
    }
    sqlite3_finalize(stmt);

    return stats;
}

bool CacheStore::setSetting(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    std::string nowIso = GeoUtils::getCurrentUtcIso();
    const char* sql = "INSERT OR REPLACE INTO settings (key, value, updated_at_iso) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nowIso.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::optional<std::string> CacheStore::getSetting(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    const char* sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return val;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

} // namespace bushfire
