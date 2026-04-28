#pragma once

#include <memory>
#include <atomic>
#include "config.h"

namespace llmproxy
{

namespace config {

class ConfigManager {
public:
    static ConfigManager& instance();

    // Initialize with config path (call once at startup)
    void init(const std::string& config_path);

    // Get current configuration
    std::shared_ptr<const ProxyConfig> get() const;

    // Reload configuration from file; returns true on success, false on error
    // On failure, old config remains unchanged
    bool reload();

    // Set config path
    void setConfigPath(const std::string& path);

    // Parse command line arguments and override config accordingly.
    // Supported: --config <path>, --port <int>, --log-level <string>
    void overrideFromArgs(int argc, char* arg[]);

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::atomic<std::shared_ptr<const ProxyConfig>> m_config;
    std::string m_configPath;
};


}  // namespace config

}  // namespace llmproxy
