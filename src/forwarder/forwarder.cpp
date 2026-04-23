#include <thread>
#include <chrono>
#include <regex>
#include <httplib.h>
#include "forwarder/forwarder.h"
#include "utils/logger.h"


namespace llmproxy
{

Forwarder::Forwarder(const config::BackendConfig& backend_config)
    : m_config(backend_config) {}

bool Forwarder::parseBackendUrl(const std::string& url, std::string& host, int& port, std::string& scheme) {
    // Regex to parse http://host:port or https://host:port
    std::regex re(R"(^(http|https)://([^:/]+)(?::(\d+))?)");
    std::smatch match;
    if (!std::regex_match(url, match, re)) {
        return false;
    }
    scheme = match[1].str();
    host = match[2].str();
    if (match.size() > 3 && match[3].matched) {
        port = std::stoi(match[3].str());
    } else {
        port = (scheme == "https") ? 443 : 80;
    }
    return true;
}

bool Forwarder::forward(const ChatRequest& req, const std::string& raw_body, std::string& out_resp_body, std::string& error_msg) {
    // Parse backend URL
    std::string host;
    int port;
    std::string scheme;
    if (!parseBackendUrl(m_config.url, host, port, scheme)) {
        error_msg = "Invalid backend URL: " + m_config.url;
        return false;
    }

    // For MVP, we only support http
    if (scheme != "http") {
        error_msg = "Only HTTP backend is supported in MVP";
        return false;
    }

    // Exponential backoff parameters
    int max_retries = m_config.max_retries;
    int initial_delay_ms = 100;
    int max_delay_ms = 1000;
    int current_delay_ms = initial_delay_ms;

    // first attempt + max_retries retries
    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        if (attempt > 0) {
            // Wait before retry
            std::this_thread::sleep_for(std::chrono::milliseconds(current_delay_ms));
            current_delay_ms = std::min(current_delay_ms * 2, max_delay_ms);
            Logger::debug("Retrying request to backend (attempt " + std::to_string(attempt + 1) + ")");
        }

        // Create HTTP client for each attempt (simple impl)
        httplib::Client client(host, port);
        // Set timeouts from config
        client.set_connection_timeout(std::chrono::milliseconds(m_config.timeout_ms));
        client.set_read_timeout(std::chrono::milliseconds(m_config.timeout_ms));
        // client.set_write_timeout(std::chrono::milliseconds(m_config.timeout_ms));

        // Send POST request
        auto start = std::chrono::steady_clock::now();
        auto res = client.Post("/v1/chat/completions", raw_body, "application/json");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (!res) {
            // Network error (connection failed, timeout, etc.)
            error_msg = "Backend request failed: " + httplib::to_string(res.error());
            Logger::warn("Forward attempt " + std::to_string(attempt + 1) +
                         " failed: " + error_msg + " (took " + std::to_string(elapsed) + "ms)");
            continue;  // retry
        }

         // Got HTTP response
        int status = res->status;
        Logger::debug("Backend responded with status " + std::to_string(status) +
                      " after " + std::to_string(elapsed) + "ms");

        // Success
        if (status >= 200 && status < 300) {
            out_resp_body = res->body;
            Logger::info("Forwarded request to backend, status=" +
                         std::to_string(status) +
                         ", duration=" + std::to_string(elapsed) + "ms");
            return true;
        }

        // For client errors (4xx), do NOT retry
        // just return the error to client
        error_msg = "Backend returned " + std::to_string(status) + " error";
        out_resp_body = res->body;  // Pass through error body
        Logger::warn("Forward attempt " + std::to_string(attempt + 1) +
                     " got client error " + std::to_string(status) + ", not retrying");
        return false;

        // For 5xx errors, retry
        if (status >= 500 && status <= 599) {
            error_msg = "Backend returned " + std::to_string(status) + " error";
            Logger::warn("Forward attempt " + std::to_string(attempt + 1) +
                         " got " + std::to_string(status) + ", will retry");
            continue;
        }
    }

    // All retries exhausted
    error_msg = "All retries exhausted: " + error_msg;
    return false;
}

}  // namespace llmproxy
