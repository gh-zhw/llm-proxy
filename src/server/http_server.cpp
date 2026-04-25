#include <httplib.h>
#include <nlohmann/json.hpp>
#include "server/http_server.h"
#include "utils/logger.h"
#include "parser/request_parser.h"
#include "forwarder/forwarder.h"


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
      m_stopStats(false),
      m_host("0.0.0.0"),
      m_port(8080),
      m_statsLoggingSeconds(0) {}

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
    m_server->Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& resp) {
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

        if (m_cache) {
            std::string cached_response;
            if (m_cache->get(cache_key, cached_response)) {
                Logger::info("Cache HIT for key: " + cache_key);
                resp.set_header("X-Cache-Status", "HIT");
                resp.set_content(cached_response, "application/json");
                resp.status = 200;
                return;
            }
            resp.set_header("X-Cache-Status", "MISS");
            Logger::debug("Cache MISS for key: " + cache_key);
        }

        // 4. Forward to backend
        Forwarder forwarder(m_backendConfig);
        std::string backend_response;
        if (forwarder.forward(chat_req, req.body, backend_response, error_msg)) {
            // Cache successful response
            if (m_cache) {
                m_cache->put(cache_key, backend_response);
            }
            // Pass backend response as-is
            resp.set_content(backend_response, "application/json");
            resp.status = 200;
        } else {
            // Forwarding failed: determine appropriate HTTP status
            if (error_msg.find("timeout") != std::string::npos ||
                error_msg.find("All retries exhausted") != std::string::npos) {
                sendError(resp, "Backend timeout", 504);
            } else {
                // For 4xx or other errors, try to extract status from error_msg
                 if (error_msg.find("400") != std::string::npos) {
                    sendError(resp, "Backend bad request", 400);
                } else if (error_msg.find("404") != std::string::npos) {
                    sendError(resp, "Backend not found", 404);
                } else {
                    sendError(resp, "Bad gateway: " + error_msg, 502);
                }
            }
        }
    });

    // Root endpoint
    m_server->Get("/", [](const httplib::Request&, httplib::Response& resp) {
        resp.set_content("llm-proxy is running", "text/plain");
        resp.status = 200;
    });

    Logger::debug("HTTP routes registered");
}

void HttpServer::statsReporter() {
    const auto report_interval = std::chrono::seconds(m_statsLoggingSeconds);
    while (!m_stopStats) {
        std::this_thread::sleep_for(report_interval);
        if (m_stopStats) break;

        // Cache stats
        if (m_cache) {
            double hit_rate = m_cache->getHitRate();
            Logger::info("Cache hit rate: " + std::to_string(hit_rate * 100) + "% ("
                         + std::to_string(m_cache->size()) + " entries)");
        }

        // More server stas...

    }
}

bool HttpServer::start(const std::string& host, int port) {
    if (m_running) {
        Logger::warn("Server already running");
        return false;
    }

    m_host = host;
    m_port = port;
    setupRoutes();

    if (m_statsLoggingSeconds > 0) {
        // Start the statistics reporter thread
        m_stopStats = false;
        m_statsThread = std::thread(&HttpServer::statsReporter, this);
    }

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
    m_server->stop();  // Causes listen() to return
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    m_stopStats = true;
    if (m_statsThread.joinable()) {
        m_statsThread.join();
    }

    m_running = false;
    Logger::info("HTTP server stopped");
}

}
