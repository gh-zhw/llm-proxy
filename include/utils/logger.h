#pragma once

#include <string>

namespace llmproxy
{

class Logger {
public:
    // Initialize the logger (must be called once at startup)
    static void init();

    static void setLevel(int level);  // spdlog::level::level_enum
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
};

}