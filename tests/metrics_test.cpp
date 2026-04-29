#include <thread>
#include <gtest/gtest.h>
#include "metrics/metrics.h"

using namespace llmproxy;


TEST(MetricsTest, RecordAndSnapshot)
{
    Metrics::instance().reset();
    Metrics::instance().recordRequest(true, 1000, true);
    Metrics::instance().recordRequest(false, 2000, true);
    Metrics::instance().recordRequest(false, 3000, false);
    auto snap = Metrics::instance().snapshot();
    EXPECT_EQ(snap.total_requests, 3);
    EXPECT_EQ(snap.cache_hits, 1);
    EXPECT_EQ(snap.cache_misses, 2);
    EXPECT_EQ(snap.forward_success, 2);
    EXPECT_EQ(snap.forward_errors, 1);
    EXPECT_EQ(snap.total_latency_us, 6000);
    EXPECT_DOUBLE_EQ(snap.avg_latency_ms, 2.0); // 6000/3/1000 = 2.0 ms
    EXPECT_DOUBLE_EQ(snap.hit_rate, 1.0/3.0);
    EXPECT_DOUBLE_EQ(snap.error_rate, 1.0/3.0);
}
