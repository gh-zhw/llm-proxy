#include <httplib.h>
#include <nlohmann/json.hpp>
#include "server/http_server.h"
#include "utils/logger.h"
#include "parser/request_parser.h"


namespace llmproxy
{

// Helper to send OpenAI-compatible error response
static void sendError(httplib::Response& resp, const std::string& message, int status_code = 400) {
    nlohmann::json error_json = {
        {"error", {
            {"message", message},
            {"type", "invalid_request"},
            {"code", status_code}
        }}
    };
    resp.set_content(error_json.dump(), "application/json");
    resp.status = status_code;
}

HttpServer::HttpServer() 
    : m_server(std::make_unique<httplib::Server>()),
      m_running(false),
      m_host("0.0.0.0"),
      m_port(8080) {}

HttpServer::~HttpServer() {
    if (m_running) { stop(); }
}

void HttpServer::setupRoutes() {
    // Health check endpoint
    m_server->Get("/health", [](const httplib::Request&, httplib::Response& resp) {
        nlohmann::json jsonResp = {
            {"status", "ok"},
            {"service", "llm-proxy"}
        };
        resp.set_content(jsonResp.dump(), "application/json");
        resp.status = 200;
    });

    // Chat completions endpoint
    m_server->Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& resp) {
        // 1. Check Content-Length (max 1MB)
        auto content_length_str = req.get_header_value("Content-Length");
        if (!content_length_str.empty()) {
            size_t content_length = std::stoul(content_length_str);
            const size_t MAX_BODY_SIZE = 1024 * 1024;  // 1MB
            if (content_length > MAX_BODY_SIZE) {
                sendError(resp, "Request body too large (max 1MB)", 413);
                return;
            }
        }

        // 2. Parse request
        ChatRequest chat_req;
        std::string error_msg;
        if (!RequestParser::parse(req.body, chat_req, error_msg)) {
            sendError(resp, error_msg, 400);
            return;
        }

        // 3. Generate cache key
        std::string cache_key = RequestParser::generateCacheKey(chat_req);
        Logger::debug("Cache key: " + cache_key);

        // 4. Placeholder response (will be replaced by forwarder/cache)
        nlohmann::json placeholder = {
            {"id", "test-response"},
            {"object", "chat.completion"},
            {"created", 1234567890},
            {"model", chat_req.model},
            {"choices", {
                {
                    {"index", 0},
                    {"message", {
                        {"role", "assistant"},
                        {"content", "This is a placeholder response. The request was valid."}
                    }},
                    {"finish_reason", "stop"}
                }
            }},
            {"usage", {
                {"prompt_tokens", 0},
                {"completion_tokens", 0},
                {"total_tokens", 0}
            }}
        };
        resp.set_content(placeholder.dump(), "application/json");
        resp.status = 200;
    });

    // Root endpoint
    m_server->Get("/", [](const httplib::Request&, httplib::Response& resp) {
        resp.set_content("llm-proxy is running", "text/plain");
        resp.status = 200;
    });

    Logger::debug("HTTP routes registered");
}

bool HttpServer::start(const std::string& host, int port) {
    if (m_running) {
        Logger::warn("Server already running");
        return false;
    }

    m_host = host;
    m_port = port;
    setupRoutes();

    m_running = true;
    m_serverThread = std::thread([this]() {
        Logger::info("Http server listening on http://" + m_host + ":" + std::to_string(m_port));
        if (!m_server->listen(m_host.c_str(), m_port)) {
            Logger::error("Failed to start HTTP server on " + m_host + ":" + std::to_string(m_port));
            m_running = false;
        }
    });

    return true;
}

void HttpServer::stop() {
    if (!m_running) return;

    Logger::info("Shutting down HTTP server...");
    m_server->stop();  //This will cause listen() to return
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    m_running = false;
    Logger::info("HTTP server stopped");
}

}
