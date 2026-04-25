#include <chrono>
#include <mutex>
#include <algorithm>
#include "cache/cache.h"
#include "utils/logger.h"


namespace llmproxy
{

Cache::Cache(size_t max_entries, size_t ttl_seconds)
    : m_max_entries(max_entries), m_ttl_seconds(ttl_seconds) {
    Logger::debug("Cache initialized: max_entries=" + std::to_string(max_entries) +
                  ", ttl_seconds=" + std::to_string(ttl_seconds));
}

bool Cache::isExpired(const Entry& entry) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp).count();
    return age >= static_cast<long long>(m_ttl_seconds);
}

void Cache::touch(const std::string& key) {
    // Assumes write lock is held
    auto it = m_lru_list.begin();
    for(; it != m_lru_list.end(); ++it) {
        if (*it == key) break;
    }
    if (it != m_lru_list.end()) {
        m_lru_list.erase(it);
    }
    m_lru_list.push_front(key);
}

void Cache::evictOne() {
    // Assumes write lock is held
    if (m_lru_list.empty()) return;
    const std::string& oldest_key = m_lru_list.back();
    m_map.erase(oldest_key);
    m_lru_list.pop_back();
    Logger::debug("Cache evicted key: " + oldest_key);
}

bool Cache::get(const std::string& key, std::string& out_value) {
    m_lookups++;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_map.find(key);
    if (it == m_map.end()) {
        return false;
    }

    Entry& entry = it->second;
    if (isExpired(entry)) {
        // Expired: remove it
        lock.unlock();
        // Acquire write lock for LRU update
        std::unique_lock<std::shared_mutex> write_lock(m_mutex);
        // Re-check after acquiring write lock
        auto it2 = m_map.find(key);
        if (it2 != m_map.end() && isExpired(it2->second)) {
            // Remove from map and LRU list
            m_map.erase(it2);
            auto list_it = std::find(m_lru_list.begin(), m_lru_list.end(), key);
            if (list_it != m_lru_list.end()) {
                m_lru_list.erase(list_it);
            }
            Logger::debug("Cache expired key: " + key);
        }
        return false;
    }

    // Cache hit: move to front of LRU
    out_value = entry.value;
    lock.unlock();

    std::unique_lock<std::shared_mutex> write_lock(m_mutex);
    auto it2 = m_map.find(key);
    if (it2 != m_map.end() && isExpired(it2->second)) {
        touch(key);
    }
    m_hits++;

    return true;
}

void Cache::put(const std::string& key, std::string& value) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    // Check if key already exists
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        // Update existing entry
        it->second.value = value;
        it->second.timestamp = std::chrono::steady_clock::now();
        touch(key);
        return;
    }

    // New entry: ensure capacity
    if (m_map.size() >= m_max_entries) {
        evictOne();
    }
    // Insert new entry
    Entry entry;
    entry.value = value;
    entry.timestamp = std::chrono::steady_clock::now();
    m_map[key] = std::move(entry);
    m_lru_list.push_front(key);
    Logger::debug("Cache inserted key: " + key + ", size=" + std::to_string(m_map.size()));
}

void Cache::clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_map.clear();
    m_lru_list.clear();
    Logger::debug("Cache cleared");
}

double Cache::getHitRate() const {
    uint64_t lookups = m_lookups.load();
    if (lookups == 0) return 0.0;
    return static_cast<double>(m_hits.load()) / lookups;
}

size_t Cache::size() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_map.size();
}

}  // namespace llmproxy
