#pragma once

#include "client.h"
#include "engine/board.h"
#include "engine/search.h"
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

    // Challenge policy
    void setAcceptRated(bool v) { acceptRated_ = v; }
    void setMinTime(int s) { minTime_ = s; }
    void setMaxTime(int s) { maxTime_ = s; }
    void setMinIncrement(int s) { minInc_ = s; }

private:
    void onEvent(const json& ev);
    void handleChallenge(const json& challenge);
    void handleGameStart(const std::string& gameId);
    void handleGameFinish(const std::string& gameId);
    void playGame(const std::string& gameId);
    void processGameState(const std::string& gameId, const json& state);

    Client client_;
    std::atomic<bool> running_{true};

    // Challenge policy
    bool acceptRated_ = false;
    int minTime_ = 30;
    int maxTime_ = 1800;
    int minInc_ = 0;

    // Active games
    struct GameContext {
        Board board;
        Search search;
        std::string id;
        std::string color;
        bool ourTurn = false;
        int wtime = 0;
        int btime = 0;
    };
    std::unordered_map<std::string, GameContext> games_;
};

} // namespace bot
} // namespace chess
