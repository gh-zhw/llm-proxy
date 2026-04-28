#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <thread>
#include "config/config.h"
#include "cache/cache.h"

// Forward declaration
namespace httplib
{
    class Server;
}


namespace llmproxy
{

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Register routes before calling start()
    void setupRoutes();

    // Start the server in a separate thread (non-blocking)
    bool start(const std::string& host, int port);

    // Block until server thread exits
    void stop();

    void setCache(std::shared_ptr<Cache> cache) { m_cache = cache; }

    bool isRunning() const { return m_running; }

private:
    std::unique_ptr<httplib::Server> m_server;
    std::string m_host;
    int m_port;
    std::atomic<bool> m_running;
    std::thread m_serverThread;

    std::thread m_statsThread;           // Thread for periodic statistics output
    std::atomic<bool> m_stopStats;       // Flag to stop statistics thread

    std::shared_ptr<Cache> m_cache;

    // Statistics reporter thread function
    void statsReporter();
};

}
