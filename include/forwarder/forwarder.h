#pragma once

#include "parser/request.h"
#include "config/config.h"

namespace llmproxy
{

class Forwarder {
public:
    // Constructor takes backend configuration (url, timeout, max_retries)
    Forwarder(const config::BackendConfig& backend_config);

    // Forward a validated chat request to the backend.
    // Returns true on success, false on error with error_msg populated.
    // On success, out_response_body contains the backend's raw response.
    bool forward(const ChatRequest& req, const std::string& raw_body,
                 std::string& out_resp_body, std::string& error_msg);
private:
    config::BackendConfig m_config;

    // Helper to parse host and port from URL
    bool parseBackendUrl(const std::string& url, std::string& host, int& port, std::string& scheme);

};

}  // namespace llmproxy
