#include <httplib.h>
#include <nlohmann/json.hpp>
#include "server/http_server.h"
#include "utils/logger.h"
#include "parser/request_parser.h"
#include "forwarder/forwarder.h"
#include "metrics/metrics.h"


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
    // Root endpoint
    m_server->Get("/", [](const httplib::Request&, httplib::Response& resp) {
        resp.set_content("llm-proxy is running", "text/plain");
        resp.status = 200;
    });

    Logger::debug("HTTP routes registered");

    // Health check endpoint
    m_server->Get("/health", [](const httplib::Request&, httplib::Response& resp) {
        nlohmann::json jsonResp = {
            {"status", "ok"},
            {"service", "llm-proxy"}
        };
        resp.set_content(jsonResp.dump(), "application/json");
        resp.status = 200;
    });

    // Metrics endpoint
    m_server->Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
        auto snap = Metrics::instance().snapshot();
        std::string metrics_text = 
            "# HELP llm_proxy_total_requests Total number of requests\n"
            "# TYPE llm_proxy_total_requests counter\n"
            "llm_proxy_total_requests " + std::to_string(snap.total_requests) + "\n"
            "# HELP llm_proxy_cache_hits Total cache hits\n"
            "# TYPE llm_proxy_cache_hits counter\n"
            "llm_proxy_cache_hits " + std::to_string(snap.cache_hits) + "\n"
            "# HELP llm_proxy_cache_misses Total cache misses\n"
            "# TYPE llm_proxy_cache_misses counter\n"
            "llm_proxy_cache_misses " + std::to_string(snap.cache_misses) + "\n"
            "# HELP llm_proxy_forward_success Successful forwards\n"
            "# TYPE llm_proxy_forward_success counter\n"
            "llm_proxy_forward_success " + std::to_string(snap.forward_success) + "\n"
            "# HELP llm_proxy_forward_errors Failed forwards\n"
            "# TYPE llm_proxy_forward_errors counter\n"
            "llm_proxy_forward_errors " + std::to_string(snap.forward_errors) + "\n"
            "# HELP llm_proxy_avg_latency_ms Average latency in milliseconds\n"
            "# TYPE llm_proxy_avg_latency_ms gauge\n"
            "llm_proxy_avg_latency_ms " + std::to_string(snap.avg_latency_ms) + "\n";
        res.set_content(metrics_text, "text/plain; version=0.0.4");
    });

    // Chat completions endpoint
    m_server->Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& resp) {
        auto start_time = std::chrono::steady_clock::now();

        // Helper to record metrics before returning
        auto record_metrics = [&](bool cache_hit, bool forward_success) {
            auto end_time = std::chrono::steady_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            Metrics::instance().recordRequest(cache_hit, latency_us, forward_success);
        };

        // 1. Check Content-Length (max 1MB)
        auto content_length_str = req.get_header_value("Content-Length");
        if (!content_length_str.empty()) {
            size_t content_length = std::stoul(content_length_str);
            const size_t MAX_BODY_SIZE = 1024 * 1024;  // 1MB
            if (content_length > MAX_BODY_SIZE) {
                sendError(resp, "Request body too large (max 1MB)", 413);
                record_metrics(false, false);
                return;
            }
        }

        // 2. Parse request
        ChatRequest chat_req;
        std::string error_msg;
        if (!RequestParser::parse(req.body, chat_req, error_msg)) {
            sendError(resp, error_msg, 400);
            record_metrics(false, false);
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
                record_metrics(true, true);
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
            record_metrics(false, true);
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
            record_metrics(false, false);
        }
    });
}

void HttpServer::statsReporter() {
    const auto report_interval = std::chrono::seconds(m_statsLoggingSeconds);
    auto last_snapshot = Metrics::instance().snapshot();
    auto last_time = std::chrono::steady_clock::now();

    while (!m_stopStats) {
        std::this_thread::sleep_for(report_interval);
        if (m_stopStats) break;

        auto now = std::chrono::steady_clock::now();
        auto cur_snapshot = Metrics::instance().snapshot();

        double elapsed_sec = std::chrono::duration<double>(now - last_time).count();
        uint64_t delta_requests = cur_snapshot.total_requests - last_snapshot.total_requests;
        double qps = delta_requests / elapsed_sec;

        Logger::info("Metrics: QPS=" + std::to_string(qps) +
                     ", avg_latency=" + std::to_string(cur_snapshot.avg_latency_ms) + "ms" +
                     ", hit_rate=" + std::to_string(cur_snapshot.hit_rate) +
                     ", error_rate=" + std::to_string(cur_snapshot.error_rate) +
                     ", total_requests=" + std::to_string(cur_snapshot.total_requests));

        last_snapshot = cur_snapshot;
        last_time = now;
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
