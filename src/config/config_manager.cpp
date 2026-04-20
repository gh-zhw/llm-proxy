#include <iostream>
#include <string>
#include <cstring>
#include "config/config.h"
#include <yaml-cpp/yaml.h>


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

void overrideFromArgs(int argc, char* argv[], ProxyConfig& config, std::string& configPath) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.server.port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            config.logging.level = argv[++i];
        }
    }
}

}  // namespace config

}  // namespace llmproxy
