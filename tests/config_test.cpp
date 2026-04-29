#include <fstream>
#include <thread>
#include <gtest/gtest.h>
#include "config/config_manager.h"

using namespace llmproxy::config;

class ConfigTest : public ::testing::Test
{
protected:
    void SetUp() override {
        // Create a temporary config file
        std::ofstream tmp("/tmp/test_proxy.yaml");
        tmp << R"(
server:
  port: 9090
  listen_address: "127.0.0.1"
backend:
  url: "http://test:1234"
  timeout_ms: 5000
  max_retries: 3
cache:
  enabled: false
  max_size_mb: 128
  ttl_seconds: 7200
logging:
  level: "debug"
)";
        tmp.close();
        ConfigManager::instance().init("/tmp/test_proxy.yaml");
    }

    void TearDown() override {
        std::remove("/tmp/test_proxy.yaml");
    }
};

TEST_F(ConfigTest, LoadFromFile)
{
    auto cfg = ConfigManager::instance().get();
    EXPECT_EQ(cfg->server.port, 9090);
    EXPECT_EQ(cfg->server.listen_address, "127.0.0.1");
    EXPECT_EQ(cfg->backend.url, "http://test:1234");
    EXPECT_EQ(cfg->backend.timeout_ms, 5000);
    EXPECT_EQ(cfg->backend.max_retries, 3);
    EXPECT_FALSE(cfg->cache.enabled);
    EXPECT_EQ(cfg->cache.max_size_mb, 128);
    EXPECT_EQ(cfg->cache.ttl_seconds, 7200);
    EXPECT_EQ(cfg->logging.level, "debug");
}

TEST_F(ConfigTest, Reload) {
    // Modify the file
    std::ofstream tmp("/tmp/test_proxy.yaml");
    tmp << R"(
server:
  port: 8888
backend:
  url: "http://new:9999"
cache:
  enabled: true
logging:
  level: "warn"
)";
    tmp.close();
    EXPECT_TRUE(ConfigManager::instance().reload());
    auto cfg = ConfigManager::instance().get();
    EXPECT_EQ(cfg->server.port, 8888);
    EXPECT_EQ(cfg->backend.url, "http://new:9999");
    EXPECT_TRUE(cfg->cache.enabled);
    EXPECT_EQ(cfg->logging.level, "warn");
    // Other fields should retain defaults (e.g., timeout_ms)
    EXPECT_EQ(cfg->backend.timeout_ms, 5000);
}

TEST_F(ConfigTest, MissingFileFallback) {
    ConfigManager::instance().init("/nonexistent.yaml");
    auto cfg = ConfigManager::instance().get();
    // Should fall back to defaults
    EXPECT_EQ(cfg->server.port, 8080);
    EXPECT_EQ(cfg->backend.url, "http://localhost:11434");
}
