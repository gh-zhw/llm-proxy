#pragma once

#include <atomic>
#include <cstdint>


namespace llmproxy
{

class Metrics {
public:
    static Metrics& instance();

    void recordRequest(bool cache_hit, int64_t latency_us, bool forward_success);

    struct Snapshot {
        uint64_t total_requests;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t forward_success;
        uint64_t forward_errors;
        uint64_t total_latency_us;
        double avg_latency_ms;
        double hit_rate;
        double error_rate;
    };

    Snapshot snapshot() const;

    void reset();

private:
    Metrics() = default;
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    std::atomic<uint64_t> m_total_requests{0};
    std::atomic<uint64_t> m_cache_hits{0};
    std::atomic<uint64_t> m_cache_misses{0};
    std::atomic<uint64_t> m_forward_success{0};
    std::atomic<uint64_t> m_forward_errors{0};
    std::atomic<uint64_t> m_total_latency_us{0};
};

}  // namespace llmproxy
