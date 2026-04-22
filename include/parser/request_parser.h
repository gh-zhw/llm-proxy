#pragma once

#include "request.h"

namespace llmproxy
{

class RequestParser {
public:
    // Parse JSON body into ChatRequest.
    // Returns true on success, false on error with error_msg populated.
    static bool parse(const std::string& body, ChatRequest& out, std::string& error_msg);

    // Generate a unique cache key from the request.
    static std::string generateCacheKey(const ChatRequest& req);
};

}  // namespace llmproxy


