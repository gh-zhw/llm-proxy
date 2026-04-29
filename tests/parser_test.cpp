#include <gtest/gtest.h>
#include "parser/request_parser.h"
#include "parser/request.h"

using namespace llmproxy;

TEST(ParserTest, ValidRequest)
{
    std::string body = R"({
                            "model": "gpt-3.5",
                            "messages": [{"role":"user","content":"Hello"}]
                        })";
    ChatRequest req;
    std::string err;
    EXPECT_TRUE(RequestParser::parse(body, req, err));
    EXPECT_EQ(req.model, "gpt-3.5");
    EXPECT_EQ(req.messages.size(), 1);
    EXPECT_EQ(req.messages[0].role, "user");
    EXPECT_EQ(req.messages[0].content, "Hello");
    EXPECT_FALSE(req.stream);
    EXPECT_FLOAT_EQ(req.temperature, 1.0f);
    EXPECT_EQ(req.max_tokens, 0);
}

TEST(ParserTest, MissingModel) {
    std::string body = R"({"messages":[{"role":"user","content":"Hi"}]})";
    ChatRequest req;
    std::string err;
    EXPECT_FALSE(RequestParser::parse(body, req, err));
    EXPECT_EQ(err, "Missing required field: model");
}

TEST(ParserTest, EmptyMessages) {
    std::string body = R"({"model":"x","messages":[]})";
    ChatRequest req;
    std::string err;
    EXPECT_FALSE(RequestParser::parse(body, req, err));
    EXPECT_EQ(err, "Messages array is empty");
}

TEST(ParserTest, StreamNotSupported) {
    std::string body = R"({
                            "model": "x",
                            "messages":[{"role":"user","content":"x"}],
                            "stream": true
                        })";
    ChatRequest req;
    std::string err;
    EXPECT_FALSE(RequestParser::parse(body, req, err));
    EXPECT_EQ(err, "Streaming is not supported in MVP");
}

TEST(ParserTest, CacheKeyDeterministic) {
    ChatRequest req1;
    req1.model = "gpt";
    req1.messages.push_back({"user", "Hi"});
    req1.temperature = 0.7f;
    req1.max_tokens = 100;
    ChatRequest req2 = req1;
    std::string key1 = RequestParser::generateCacheKey(req1);
    std::string key2 = RequestParser::generateCacheKey(req2);
    EXPECT_EQ(key1, key2);
}
