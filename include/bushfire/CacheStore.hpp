#pragma once

#include "bushfire/Types.hpp"
#include <string>
#include <optional>
#include <mutex>
#include <sqlite3.h>

namespace bushfire {

struct CacheEntry {
    std::string key;
    std::string payload;
    std::string fetchedAtIso;
    long expiresAtEpoch{0};
    std::string etag;
    std::string lastModified;
    FreshnessStatus freshness{FreshnessStatus::Error};
};

struct CacheStats {
    size_t totalEntries{0};
    size_t staleEntries{0};
    size_t freshEntries{0};
    size_t totalBytes{0};
};

class CacheStore {
public:
    explicit CacheStore(const std::string& dbPath = "bushfire_cache.sqlite3");
    ~CacheStore();

    // Non-copyable
    CacheStore(const CacheStore&) = delete;
    CacheStore& operator=(const CacheStore&) = delete;

    // Cache operations
    bool set(const std::string& key, const std::string& payload, int ttlSeconds,
             const std::string& etag = "", const std::string& lastModified = "");
    
    std::optional<CacheEntry> get(const std::string& key);

    bool remove(const std::string& key);
    bool clear();
    size_t evictExpired();

    CacheStats getStats();

    // Settings operations (safe storage for API keys and user preferences)
    bool setSetting(const std::string& key, const std::string& value);
    std::optional<std::string> getSetting(const std::string& key);

private:
    void initSchema();

    std::string dbPath_;
    sqlite3* db_{nullptr};
    std::mutex mutex_;
};

} // namespace bushfire
