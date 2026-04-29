#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "utils/logger.h"
#include "config/config_manager.h"
#include "server/http_server.h"
#include "cache/cache.h"


// Global flag for signal handling
static std::atomic<bool> g_shutdown_requested(false);
static std::atomic<bool> g_reload_config(false);

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        std::string signal_str = signal == SIGINT ? "SIGINT" : "SIGTERM";
        llmproxy::Logger::info("Received " + signal_str + ", shutting down...");
        g_shutdown_requested = true;
    } else if (signal == SIGHUP) {
        g_reload_config = true;
        llmproxy::Logger::info("Received SIGHUP, will reload config");
    }
}

// Helper: convert string log level to spdlog level
static spdlog::level::level_enum strToSpdlogLevel(const std::string& level) {
    if (level == "debug") return spdlog::level::debug;
    if (level == "info")  return spdlog::level::info;
    if (level == "warn")  return spdlog::level::warn;
    if (level == "error") return spdlog::level::err;
    return spdlog::level::info;
}

static void printConfigSummary(std::shared_ptr<const llmproxy::config::ProxyConfig> config) {
    llmproxy::Logger::info("Configuration loaded:");
    llmproxy::Logger::info("  Server: " + config->server.listen_address + 
                           ":" + std::to_string(config->server.port));
    llmproxy::Logger::info("  Backend: " + config->backend.url);
    llmproxy::Logger::info("  Cache enabled: " + std::string(config->cache.enabled ? "true" : "false"));
    llmproxy::Logger::info("  Log level: " + config->logging.level);
}


int main(int argc, char* argv[])
{
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);

    // Load configuration
    std::string configPath = "./config/proxy.yaml";
    auto& cfgManager = llmproxy::config::ConfigManager::instance();
    cfgManager.init(configPath);
    cfgManager.overrideFromArgs(argc, argv);
    auto config = cfgManager.get();

    // Initialize logger with config's log level
    llmproxy::Logger::init();
    llmproxy::Logger::setLevel(strToSpdlogLevel(config->logging.level));

    printConfigSummary(config);

    // Create HTTP server
    llmproxy::HttpServer server;

    // Create cache instance if enabled
    std::shared_ptr<llmproxy::Cache> cache(nullptr);
    if (config->cache.enabled) {
        // Convert max_size_mb to approximate max_entries
        // Assume average response size = 4KB (4096 bytes)
        size_t max_entries = (config->cache.max_size_mb * 1024 * 1024) / 4096;
        cache = std::make_shared<llmproxy::Cache>(max_entries, config->cache.ttl_seconds);
        server.setCache(cache);
    }

    if (!server.start(config->server.listen_address, config->server.port)) {
        llmproxy::Logger::error("Failed to start Http server, exiting.");
        return 1;
    }

    // Wait for shutdown signal
    while (!g_shutdown_requested) {
        if (g_reload_config) {
            g_reload_config = false;
            // Reload configurations
            if (cfgManager.reload()) {
                config = cfgManager.get();

                // Update logger level
                llmproxy::Logger::setLevel(strToSpdlogLevel(config->logging.level));
                
                if (cache) {
                    // Update cache parameters
                    cache->resize((config->cache.max_size_mb * 1024 * 1024) / 4096);
                    cache->setTtl(config->cache.ttl_seconds);
                }
                llmproxy::Logger::info("Configuration reload applied successfully");
            } else {
                llmproxy::Logger::error("Configuration reload failed, keeping old settings");
            }
         }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Shutdown HTTP server
    server.stop();
    llmproxy::Logger::info("llm-proxy stopped.");

    return 0;
}
