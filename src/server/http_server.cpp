#include <httplib.h>
#include <nlohmann/json.hpp>
#include "server/http_server.h"
#include "utils/logger.h"


namespace llmproxy
{

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

    // Placeholder for chat completions
    m_server->Post("/v1/chat/completions", [](const httplib::Request&, httplib::Response& resp) {
        nlohmann::json jsonResp = {
            {"message", "placeholder - not implemented yet"}
        };
        resp.set_content(jsonResp.dump(), "application/json");
        resp.status = 200;
    });

    // Test: a simple root endpoint
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
