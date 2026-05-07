#pragma once

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <functional>
#include <optional>

namespace chess {
namespace bot {

using json = nlohmann::json;

// Callback for NDJSON stream lines
using StreamCallback = std::function<void(const json&)>;

class Client {
public:
    Client(const std::string& token);
    ~Client();

    void setDebug(bool v) { debug_ = v; }

    // Account
    json getAccount();
    json getOnlineBots(int nb = 100);

    // Challenges
    json acceptChallenge(const std::string& challengeId);
    json declineChallenge(const std::string& challengeId, const std::string& reason = "");
    json createChallenge(const std::string& username, const json& params);

    // Game actions
    json makeMove(const std::string& gameId, const std::string& uciMove, bool drawOffer = false);
    json resignGame(const std::string& gameId);
    json abortGame(const std::string& gameId);
    json sendChat(const std::string& gameId, const std::string& room, const std::string& text);
    json claimVictory(const std::string& gameId);

    // Streaming
    void streamEvents(StreamCallback callback);
    void streamGame(const std::string& gameId, StreamCallback callback);

    // Upgrade account to bot
    json upgradeToBot();

private:
    std::string get(const std::string& url);
    std::string post(const std::string& url, const std::string& data = "");
    void streamGet(const std::string& url, StreamCallback callback);

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    static size_t streamLineCallback(void* contents, size_t size, size_t nmemb, void* userp);

    std::string token_;
    std::string baseUrl_;
    bool debug_ = false;
};

} // namespace bot
} // namespace chess
