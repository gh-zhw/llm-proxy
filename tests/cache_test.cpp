#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "cache/cache.h"

using namespace llmproxy;

TEST(CacheTest, PutAndGet)
{
    Cache cache(10, 60);
    cache.put("key", "value");
    std::string val;
    EXPECT_TRUE(cache.get("key", val));
    EXPECT_EQ(val, "value");
}

TEST(CacheTest, ExpiredEntry)
{
    Cache cache(10, 1);  // TTL 1 second
    cache.put("key", "value");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::string val;
    EXPECT_FALSE(cache.get("key", val));
}

TEST(CacheTest, LRUEviction)
{
    Cache cache(2, 60);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");  // should evict "a"
    std::string val;
    EXPECT_FALSE(cache.get("a", val));
    EXPECT_TRUE(cache.get("b", val));
    EXPECT_TRUE(cache.get("c", val));
}

TEST(CacheTest, UpdateExisting)
{
    Cache cache(10, 60);
    cache.put("key", "old");
    cache.put("key", "new");
    std::string val;
    cache.get("key", val);
    EXPECT_EQ(val, "new");
}

TEST(CacheTest, Resize)
{
    Cache cache(3, 60);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");
    cache.resize(2);
    std::string val;
    EXPECT_FALSE(cache.get("a", val));
    EXPECT_TRUE(cache.get("b", val));
    EXPECT_TRUE(cache.get("c", val));
}

