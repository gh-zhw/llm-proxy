#include "metrics/metrics.h"


namespace llmproxy
{

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

void Metrics::recordRequest(bool cache_hit, int64_t latency_us, bool forward_success) {
    m_total_requests++;
    m_total_latency_us += latency_us;

    if (cache_hit) {
        m_cache_hits++;
    } else {
        m_cache_misses++;
    }

    if (forward_success) {
        m_forward_success++;
    } else {
        m_forward_errors++;
    }
}

Metrics::Snapshot Metrics::snapshot() const {
    uint64_t total = m_total_requests.load(std::memory_order_relaxed);
    uint64_t hits = m_cache_hits.load(std::memory_order_relaxed);
    uint64_t misses = m_cache_misses.load(std::memory_order_relaxed);
    uint64_t fwd_success = m_forward_success.load(std::memory_order_relaxed);
    uint64_t fwd_errors = m_forward_errors.load(std::memory_order_relaxed);
    uint64_t total_lat = m_total_latency_us.load(std::memory_order_relaxed);

    double avg_lat_ms = 0.0;
    double hit_rate = 0.0;
    double error_rate = 0.0;

    if (total > 0) {
        avg_lat_ms = static_cast<double>(total_lat) / total / 1000.0;
        hit_rate = static_cast<double>(hits) / total;
        error_rate = static_cast<double>(fwd_errors) / total;
    }

    return Snapshot{total, hits, misses,
                    fwd_success, fwd_errors, total_lat,
                    avg_lat_ms, hit_rate, error_rate};
}

void Metrics::reset() {
    m_total_requests = 0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_forward_success = 0;
    m_forward_errors = 0;
    m_total_latency_us = 0;
}


}  // namespace llmproxy
