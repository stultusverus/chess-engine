#pragma once

#include "client.h"
#include "engine/board.h"
#include "engine/search.h"
#include "engine/book.h"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <functional>
#include <unordered_map>

namespace chess {
namespace bot {

class Manager {
public:
    Manager(const std::string& token);
    ~Manager();

    void run();
    void challengeOpponent(const std::string& username, int clockLimit, int clockInc, bool rated = false);
    void challengeBots(int count = 1, int clockLimit = 0, int clockInc = 0, bool rated = false);
    void setDebug(bool v);

    // Challenge policy
    void setAcceptRated(bool v) { acceptRated_ = v; }
    void setMinTime(int s) { minTime_ = s; }
    void setMaxTime(int s) { maxTime_ = s; }
    void setMinIncrement(int s) { minInc_ = s; }
    void setBookPath(const std::string& path);

    void setAutoResign(bool v) { autoResign_ = v; }
    void setResignThreshold(int cp) { resignThreshold_ = cp; }
    void setAutoDraw(bool v) { autoDraw_ = v; }
    void setDrawThreshold(int cp) { drawEvalThreshold_ = cp; }
    void setDrawOfferMoves(int n) { drawOfferMoves_ = n; }

    // Active games
    struct GameContext {
        Board board;
        Search search;
        std::string id;
        std::string color;
        std::string initialFen;
        bool ourTurn = false;
        int wtime = 0;
        int btime = 0;
        bool drawOffered = false;
        int consecutiveDrawishMoves = 0;
    };

private:
    void onEvent(const json& ev);
    void handleChallenge(const json& challenge);
    void handleGameStart(const std::string& gameId);
    void handleGameFinish(const std::string& gameId);
    void playGame(const std::string& gameId);
    void processGameState(const std::string& gameId, const json& state);
    bool tryBookMove(const std::string& gameId, GameContext& ctx);
    bool maybeResignOrDraw(const std::string& gameId, GameContext& ctx, const chess::SearchResult& result);

    void enqueue(std::function<void()> task);
    void workerLoop();

    Client client_;
    std::atomic<bool> running_{true};
    bool debug_ = false;

    // Worker thread for HTTP actions (must not call curl from stream callbacks)
    std::thread workerThread_;
    std::deque<std::function<void()>> actionQueue_;
    std::mutex actionMutex_;

    // Opening book
    Book book_;

    // Shortcut for game-prefixed debug output
    void dbg(const std::string& gameId, const std::string& msg) const {
        if (debug_) std::cerr << "[" << gameId << "] " << msg << std::endl;
    }

    // Challenge policy
    bool acceptRated_ = false;
    int minTime_ = 30;
    int maxTime_ = 1800;
    int minInc_ = 0;

    // Resign / draw policy
    bool autoResign_ = true;
    int resignThreshold_ = -800;
    bool autoDraw_ = true;
    int drawEvalThreshold_ = 20;
    int drawOfferMoves_ = 4;

    // Active games
    std::unordered_map<std::string, GameContext> games_;
};

} // namespace bot
} // namespace chess
