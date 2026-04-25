#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <chrono>
#include <shared_mutex>
#include <atomic>

namespace llmproxy
{

class Cache {
public:
    // max_entries: maximum number of entries (LRU eviction)
    // ttl_seconds: time-to-live for each entry in seconds
    Cache(size_t max_entries, size_t ttl_seconds);
    ~Cache() = default;

    // Get cached response by key. Returns true if found and not expired.
    // On hit, writes response into out_value.
    bool get(const std::string& key, std::string& out_value);

    // Insert or update a cache entry.
    void put(const std::string& key, std::string& value);

    // Clear all entries
    void clear();

    // Get current hit rate (hits / lookups)
    double getHitRate() const;

    // Get current size
    size_t size() const;

private:
    struct Entry {
        std::string value;
        std::chrono::steady_clock::time_point timestamp;
    };

    size_t m_max_entries;
    size_t m_ttl_seconds;

    // Main data structures
    std::unordered_map<std::string, Entry> m_map;
    std::list<std::string> m_lru_list;  // front == most recently used

    mutable std::shared_mutex m_mutex;

    // Statistics
    mutable std::atomic<uint64_t> m_hits{0};
    mutable std::atomic<uint64_t> m_lookups{0};

    // Helper to touch an existing key (move to front of LRU)
    void touch(const std::string& key);

    // Evict least recently used entry
    void evictOne();

    // Check if an entry is expired
    bool isExpired(const Entry& entry) const;
};

}  // namespace llmproxy
