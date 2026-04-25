#pragma once

#include <string>


namespace llmproxy
{

namespace config {

struct ServerConfig {
    int port = 8080;
    std::string listen_address = "0.0.0.0";
    int stats_logging_seconds = 60;
};

struct BackendConfig {
    std::string url = "http://localhost:11434";
    int timeout_ms = 30000;
    int max_retries = 2;
};

struct CacheConfig {
    bool enabled = true;
    int max_size_mb = 256;
    int ttl_seconds = 3600;
};

struct LoggingConfig {
    std::string level = "info";  // debug, info, warn, error
};

struct ProxyConfig {
    ServerConfig server;
    BackendConfig backend;
    CacheConfig cache;
    LoggingConfig logging;
};

// Load configuration from YAML file, with fallback to defaults.
// Returns config with values from: command line > file > default.
ProxyConfig loadConfig(const std::string& filepath);

// Parse command line arguments and override config accordingly.
// Supported: --config <path>, --port <int>, --log-level <string>
void overrideFromArgs(int argc, char* arg[], ProxyConfig& config, std::string& filepath);

}  // namespace config

}  // namespace llmproxy
