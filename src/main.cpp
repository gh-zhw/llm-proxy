#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "utils/logger.h"
#include "config/config.h"
#include "server/http_server.h"
#include <spdlog/spdlog.h>

// Global flag for signal handling
static std::atomic<bool> g_shutdown_requested(false);

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        llmproxy::Logger::info("Received signal " + std::to_string(signal) + ", shutting down...");
        g_shutdown_requested = true;
    }
}

int main(int argc, char* argv[])
{
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Load configuration
    std::string configPath = "./config/proxy.yaml";
    auto config = llmproxy::config::loadConfig(configPath);
    llmproxy::config::overrideFromArgs(argc, argv, config, configPath);

    // If config path changed, reload
    if (configPath != "./config/proxy.yaml") {
        config = llmproxy::config::loadConfig(configPath);
        llmproxy::config::overrideFromArgs(argc, argv, config, configPath);
    }

     // Initialize logger with config's log level
    llmproxy::Logger::init();
    int spdlog_level = spdlog::level::info;
    if (config.logging.level == "debug") spdlog_level = spdlog::level::debug;
    else if (config.logging.level == "warn") spdlog_level = spdlog::level::warn;
    else if (config.logging.level == "error") spdlog_level = spdlog::level::err;
    llmproxy::Logger::setLevel(spdlog_level);

    // Print configuration summary
    llmproxy::Logger::info("Configuration loaded:");
    llmproxy::Logger::info("  Server: " + config.server.listen_address + ":" + std::to_string(config.server.port));
    llmproxy::Logger::info("  Backend: " + config.backend.url);
    llmproxy::Logger::info("  Cache enabled: " + std::string(config.cache.enabled ? "true" : "false"));
    llmproxy::Logger::info("  Log level: " + config.logging.level);

    // Create and start HTTP server
    llmproxy::HttpServer server;
    if (!server.start(config.server.listen_address, config.server.port)) {
        llmproxy::Logger::error("Failed to start Http server, exiting.");
        return 1;
    }

    // Wait for shutdown signal
    while (!g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Shutdown HTTP server
    server.stop();
    llmproxy::Logger::info("llm-proxy stopped.");

    return 0;
}
