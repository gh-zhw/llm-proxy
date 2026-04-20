#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "utils/logger.h"


namespace llmproxy
{

static std::shared_ptr<spdlog::logger> s_logger;

void Logger::init() {
    if (s_logger) return;  // already initialized

    s_logger = spdlog::stdout_color_mt("llm-proxy");

    // Set log format: [2025-04-20 10:30:00.123] [info] message
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    s_logger->set_level(spdlog::level::info); // default level
}

void Logger::setLevel(int level) {
    if (s_logger) {
        s_logger->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::debug(const std::string& msg) {
    if (s_logger) s_logger->debug(msg);
}

void Logger::info(const std::string& msg) {
    if (s_logger) s_logger->info(msg);
}

void Logger::warn(const std::string& msg) {
    if (s_logger) s_logger->warn(msg);
}

void Logger::error(const std::string& msg) {
    if (s_logger) s_logger->error(msg);
}


}
