#include "client.h"
#include <iostream>
#include <sstream>

namespace chess {
namespace bot {

Client::Client(const std::string& token)
    : token_(token), baseUrl_("https://lichess.org") {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Client::~Client() {
    curl_global_cleanup();
}

size_t Client::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

std::string Client::get(const std::string& url) {
    if (debug_) std::cerr << "[http] GET " << url << std::endl;

    CURL* curl = curl_easy_init();
    std::string response;
    long httpCode = 0;
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "chess-engine/1.0");
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    if (debug_) std::cerr << "[http]  -> " << httpCode << " (" << response.size() << " bytes)" << std::endl;
    return response;
}

std::string Client::post(const std::string& url, const std::string& data) {
    if (debug_) {
        std::cerr << "[http] POST " << url;
        if (!data.empty()) std::cerr << " body=" << data;
        std::cerr << std::endl;
    }

    CURL* curl = curl_easy_init();
    std::string response;
    long httpCode = 0;
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "chess-engine/1.0");
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    if (debug_) std::cerr << "[http]  -> " << httpCode << " (" << response.size() << " bytes)" << std::endl;
    return response;
}

struct StreamContext {
    StreamCallback callback;
    std::string buffer;
    bool debug = false;
};

size_t Client::streamLineCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    StreamContext* ctx = static_cast<StreamContext*>(userp);
    size_t total = size * nmemb;
    ctx->buffer.append(static_cast<char*>(contents), total);

    size_t pos;
    while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
        std::string line = ctx->buffer.substr(0, pos);
        ctx->buffer.erase(0, pos + 1);

        if (!line.empty()) {
            try {
                json j = json::parse(line);
                if (ctx->debug) std::cerr << "[stream] " << j.dump() << std::endl;
                ctx->callback(j);
            } catch (...) {
                if (ctx->debug) std::cerr << "[stream] (unparseable) " << line << std::endl;
            }
        }
    }
    return total;
}

void Client::streamGet(const std::string& url, StreamCallback callback) {
    if (debug_) std::cerr << "[stream] connecting to " << url << std::endl;

    CURL* curl = curl_easy_init();
    if (curl) {
        StreamContext ctx{callback, "", debug_};
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamLineCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "chess-engine/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // Infinite
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// --- API Methods ---

json Client::getAccount() {
    std::string resp = get(baseUrl_ + "/api/account");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::acceptChallenge(const std::string& challengeId) {
    std::string resp = post(baseUrl_ + "/api/challenge/" + challengeId + "/accept");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::declineChallenge(const std::string& challengeId, const std::string& reason) {
    json data;
    if (!reason.empty()) data["reason"] = reason;
    std::string resp = post(baseUrl_ + "/api/challenge/" + challengeId + "/decline", data.dump());
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::createChallenge(const std::string& username, const json& params) {
    std::string resp = post(baseUrl_ + "/api/challenge/" + username, params.dump());
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::makeMove(const std::string& gameId, const std::string& uciMove, bool drawOffer) {
    std::string url = baseUrl_ + "/api/bot/game/" + gameId + "/move/" + uciMove;
    if (drawOffer) url += "?offeringDraw=true";
    std::string resp = post(url);
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::resignGame(const std::string& gameId) {
    std::string resp = post(baseUrl_ + "/api/bot/game/" + gameId + "/resign");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::abortGame(const std::string& gameId) {
    std::string resp = post(baseUrl_ + "/api/bot/game/" + gameId + "/abort");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::sendChat(const std::string& gameId, const std::string& room, const std::string& text) {
    json data;
    data["room"] = room;
    data["text"] = text;
    std::string resp = post(baseUrl_ + "/api/bot/game/" + gameId + "/chat", data.dump());
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

json Client::claimVictory(const std::string& gameId) {
    std::string resp = post(baseUrl_ + "/api/bot/game/" + gameId + "/claim-victory");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

void Client::streamEvents(StreamCallback callback) {
    streamGet(baseUrl_ + "/api/stream/event", callback);
}

void Client::streamGame(const std::string& gameId, StreamCallback callback) {
    streamGet(baseUrl_ + "/api/bot/game/stream/" + gameId, callback);
}

json Client::upgradeToBot() {
    std::string resp = post(baseUrl_ + "/api/bot/account/upgrade");
    if (resp.empty()) return json::object();
    return json::parse(resp);
}

} // namespace bot
} // namespace chess
