
#include "utils/logger.h"

int main()
{
    llmproxy::Logger::init();
    llmproxy::Logger::info("llm-proxy starting...");
    llmproxy::Logger::debug("This debug message will not appear (level=info)");
    llmproxy::Logger::warn("Be careful!");
    llmproxy::Logger::error("Something went wrong (example)");

    return 0;
}
