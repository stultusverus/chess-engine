#pragma once

#include "client.h"
#include "engine/board.h"
#include "engine/search.h"
#include "engine/book.h"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
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
    };

private:
    void onEvent(const json& ev);
    void handleChallenge(const json& challenge);
    void handleGameStart(const std::string& gameId);
    void handleGameFinish(const std::string& gameId);
    void playGame(const std::string& gameId);
    void processGameState(const std::string& gameId, const json& state);
    bool tryBookMove(const std::string& gameId, GameContext& ctx);

    Client client_;
    std::atomic<bool> running_{true};
    bool debug_ = false;

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

    // Active games
    std::unordered_map<std::string, GameContext> games_;
};

} // namespace bot
} // namespace chess
