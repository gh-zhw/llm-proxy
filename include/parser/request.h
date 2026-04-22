#pragma once

#include <string>
#include <vector>

namespace llmproxy
{

struct Message {
    std::string role;
    std::string content;
};

struct ChatRequest {
    std::string model;
    std::vector<Message> messages;
    float temperature = 1.0f;
    int max_tokens = 0;  // 0 means no limit
    bool stream = false;
};

}  // namespace llmproxy
