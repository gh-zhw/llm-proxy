#include <iostream>
#include <string>
#include <cstring>
#include <optional>
#include <yaml-cpp/yaml.h>
#include "config/config.h"
#include "config/config_manager.h"
#include "utils/logger.h"


namespace llmproxy
{

namespace config {

static ProxyConfig getDefaultConfig(){
    return ProxyConfig();
}

ProxyConfig loadConfig(const std::string& filepath) {
    ProxyConfig config = getDefaultConfig();

    try {
        YAML::Node root = YAML::LoadFile(filepath);

        // Server
        if (root["server"] && root["server"].IsMap()) {
            auto sv = root["server"];
            if (sv["port"] && sv["port"].IsScalar())
                config.server.port = sv["port"].as<int>();
            if (sv["listen_address"] && sv["listen_address"].IsScalar())
                config.server.listen_address = sv["listen_address"].as<std::string>();
            if (sv["stats_logging_seconds"] && sv["stats_logging_seconds"].IsScalar())
                config.server.stats_logging_seconds = sv["stats_logging_seconds"].as<int>();
        }

        // Backend
        if (root["backend"] && root["backend"].IsMap()) {
            auto be = root["backend"];
            if (be["url"] && be["url"].IsScalar())
                config.backend.url = be["url"].as<std::string>();
            if (be["timeout_ms"] && be["timeout_ms"].IsScalar())
                config.backend.timeout_ms = be["timeout_ms"].as<int>();
            if (be["max_retries"] && be["max_retries"].IsScalar())
                config.backend.max_retries = be["max_retries"].as<int>();
        }

        // Cache
        if (root["cache"] && root["cache"].IsMap()) {
            auto ca = root["cache"];
            if (ca["enabled"] && ca["enabled"].IsScalar())
                config.cache.enabled = ca["enabled"].as<bool>();
            if (ca["max_size_mb"] && ca["max_size_mb"].IsScalar())
                config.cache.max_size_mb = ca["max_size_mb"].as<int>();
            if (ca["ttl_seconds"] && ca["ttl_seconds"].IsScalar())
                config.cache.ttl_seconds = ca["ttl_seconds"].as<int>();
        }

        // Logging
        if (root["logging"] && root["logging"].IsMap()) {
            auto lg = root["logging"];
            if (lg["level"] && lg["level"].IsScalar())
                config.logging.level = lg["level"].as<std::string>();
        }

        std::cout << "[INFO] Loaded config from: " << filepath << std::endl;
    } catch(const YAML::BadFile& e) {
        std::cerr << "[WARNING] Config file not found: " << filepath 
                  << ". Using default configuration." << std::endl;
    } catch (const YAML::ParserException& e) {
        std::cerr << "[ERROR] Failed to parse YAML: " << e.what() 
                  << ". Using defaults." << std::endl;
    }

    return config;
}


ConfigManager& ConfigManager::instance() {
    static ConfigManager manager;
    return manager;
}

void ConfigManager::init(const std::string& config_path) {
    m_configPath = config_path;
    try {
        auto config = std::make_shared<const ProxyConfig>(loadConfig(m_configPath));
        m_config.store(config, std::memory_order_release);
    } catch(...) {
        // If loading fails, use defaults
        auto config = std::make_shared<const ProxyConfig>(getDefaultConfig());
        m_config.store(config, std::memory_order_release);
        std::cerr << "[ERROR] Failed to load config file: " << m_configPath 
                  << ". Using defaults." << std::endl;
    }
}

std::shared_ptr<const ProxyConfig> ConfigManager::get() const {
    return m_config.load(std::memory_order_acquire);
}

bool ConfigManager::reload() {
    Logger::info("Reloading config from " + m_configPath + "...");
    ProxyConfig config;
    try {
        config = loadConfig(m_configPath);
    } catch (...) {
        Logger::error("Config reload failed, keeping old config");
        return false;
    }
    auto sptr = std::make_shared<const ProxyConfig>(std::move(config));
    m_config.store(sptr, std::memory_order_release);
    Logger::info("Config reloaded successfully");
    return true;
}

void ConfigManager::setConfigPath(const std::string& path) {
    m_configPath = path;
}

void ConfigManager::overrideFromArgs(int argc, char* argv[]) {
    std::string configPath;
    std::optional<int> port;
    std::string level;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Invalid port number, ignored\n";
            }
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            level = argv[++i];
        }
    }

    auto sptr = m_config.load(std::memory_order_acquire);
    ProxyConfig config = sptr ? *sptr : getDefaultConfig();
    if (!configPath.empty()) {
        try {
            config = loadConfig(configPath);
        } catch (...) {
            std::cerr << "[ERROR] Failed to load config file: " << configPath 
                      << ". Using fallback configuration." << std::endl;
        }
    }
    if (port.has_value()) {
        int p = port.value();
        if (p >= 0 && p <= 65535) {
            config.server.port = p;
        } else {
            std::cerr << "Invalid port " << p << ", must be 0-65535. Ignored." << std::endl;
        }
    }
    if (!level.empty()) {
        config.logging.level = level;
    }
    sptr = std::make_shared<const ProxyConfig>(std::move(config));
    m_config.store(sptr, std::memory_order_release);
}

}  // namespace config

}  // namespace llmproxy
