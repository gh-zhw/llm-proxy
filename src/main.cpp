#include <iostream>
#include <spdlog/spdlog.h>
#include "utils/logger.h"
#include "config/config.h"

int main(int argc, char* argv[])
{
    // Default config path
    std::string configPath = "./config/proxy.yaml";

    // Load default config
    auto config = llmproxy::config::loadConfig(configPath);

    // Override from command line (including possible new config path)
    llmproxy::config::overrideFromArgs(argc, argv, config, configPath);

    // If config path was changed via --config, reload with new path
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

    // Test logs at different levels
    llmproxy::Logger::debug("This is a debug message (visible only if log level is debug)");
    llmproxy::Logger::info("llm-proxy starting...");
    llmproxy::Logger::warn("Be careful!");
    llmproxy::Logger::error("Something went wrong (example)");

    return 0;
}
