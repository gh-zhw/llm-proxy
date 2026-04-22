#include <sstream>
#include <functional>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "parser/request_parser.h"
#include "utils/logger.h"


namespace llmproxy
{

bool RequestParser::parse(const std::string& body, ChatRequest& out, std::string& error_msg) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::parse_error& e) {
        error_msg = "Invalid JSON: " + std::string(e.what());
        return false;
    }

    // Required: model
    if (!json.contains("model") || !json["model"].is_string()) {
        error_msg = "Missing required field: model";
        return false;
    }
    out.model = json["model"].get<std::string>();

    // Required: messages
    if (!json.contains("messages") || !json["messages"].is_array()) {
        error_msg = "Missing required field: messages";
        return false;
    }
    auto& msgs = json["messages"];
    if (msgs.empty()) {
        error_msg = "Messages array is empty";
        return false;
    }
    for (const auto& msg : msgs) {
        if (!msg.contains("role") || !msg["role"].is_string() ||
            !msg.contains("content") || !msg["content"].is_string()) {
            error_msg = "Each messages must have role and content (strings)";
            return false;
        }
        Message m;
        m.role = msg["role"].get<std::string>();
        m.content = msg["content"].get<std::string>();
        out.messages.push_back(m);
    }

    // Optional: temperature
    if (json.contains("temperature") && json["temperature"].is_number()) {
        out.temperature = json["temperature"].get<float>();
    }

    // Optional: max_tokens
    if (json.contains("max_tokens") && json["max_tokens"].is_number_integer()) {
        out.max_tokens = json["max_tokens"].get<int>();
    }

    // Optional: stream
    if (json.contains("stream") && json["stream"].is_boolean()) {
        out.stream = json["stream"].get<bool>();
        if (out.stream) {
            error_msg = "Streaming is not supported in MVP";
            return false;
        }
    }

    Logger::debug("Parsed request: model=" + out.model + 
                  ", messages=" + std::to_string(out.messages.size()) +
                  ", temp=" + std::to_string(out.temperature) +
                  ", max_tokens=" + std::to_string(out.max_tokens));
    return true;
}

std::string RequestParser::generateCacheKey(const ChatRequest& req) {
    // Combine hash of model, temperature, max_tokens, and messages
    std::size_t seed = std::hash<std::string>{}(req.model);
    seed ^= std::hash<float>{}(req.temperature) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(req.max_tokens) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    for (const auto& msg : req.messages) {
        std::string combined = msg.role + "\x1F" + msg.content;  // ASCII unit separator
        seed ^= std::hash<std::string>{}(combined) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    std::stringstream ss;
    ss << std::hex << std::setw(sizeof(std::size_t) * 2) << std::setfill('0') << seed;
    return ss.str();
}

}  // namespace llmproxy
