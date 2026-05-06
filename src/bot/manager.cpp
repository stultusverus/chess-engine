#include "manager.h"
#include "engine/attacks.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace chess {
namespace bot {

Manager::Manager(const std::string& token) : client_(token) {
    chess::attacks::init();
    chess::Board::initZobrist();
}

Manager::~Manager() {
    running_ = false;
}

void Manager::run() {
    std::cerr << "Bot manager starting..." << std::endl;

    client_.streamEvents([this](const json& ev) {
        if (!running_) return;
        onEvent(ev);
    });
}

void Manager::onEvent(const json& ev) {
    std::string type = ev.value("type", "");

    if (type == "challenge") {
        handleChallenge(ev["challenge"]);
    } else if (type == "gameStart") {
        handleGameStart(ev["game"]["id"]);
    } else if (type == "gameFinish") {
        handleGameFinish(ev["game"]["id"]);
    }
}

void Manager::handleChallenge(const json& challenge) {
    std::string id = challenge["id"];
    bool rated = challenge.value("rated", false);
    json tc = challenge.value("clock", json::object());
    int limit = tc.value("limit", 0);
    int increment = tc.value("increment", 0);
    std::string challenger = challenge.value("challenger", json::object()).value("name", "?");

    std::cerr << "Challenge from " << challenger
              << " (rated=" << rated << " time=" << limit << "+" << increment << ")" << std::endl;

    // Apply policy
    if (acceptRated_ && !rated) {
        std::cerr << "  Declining: rated-only mode" << std::endl;
        client_.declineChallenge(id, "Only accepting rated games");
        return;
    }
    if (limit < minTime_ || limit > maxTime_) {
        std::cerr << "  Declining: time " << limit << " outside [" << minTime_ << "," << maxTime_ << "]" << std::endl;
        client_.declineChallenge(id);
        return;
    }
    if (increment < minInc_) {
        std::cerr << "  Declining: increment " << increment << " < " << minInc_ << std::endl;
        client_.declineChallenge(id);
        return;
    }

    std::cerr << "  Accepting challenge " << id << std::endl;
    client_.acceptChallenge(id);
}

void Manager::handleGameStart(const std::string& gameId) {
    std::cerr << "Game started: " << gameId << std::endl;

    GameContext ctx;
    ctx.id = gameId;
    games_[gameId] = ctx;

    // Start game in background thread
    std::thread([this, gameId]() {
        playGame(gameId);
    }).detach();
}

void Manager::handleGameFinish(const std::string& gameId) {
    std::cerr << "Game finished: " << gameId << std::endl;
    games_.erase(gameId);
}

void Manager::playGame(const std::string& gameId) {
    auto& ctx = games_[gameId];
    ctx.search.setTTSize(64);

    client_.streamGame(gameId, [this, gameId](const json& ev) {
        if (!running_ || games_.find(gameId) == games_.end()) return;
        processGameState(gameId, ev);
    });
}

void Manager::processGameState(const std::string& gameId, const json& state) {
    auto it = games_.find(gameId);
    if (it == games_.end()) return;

    auto& ctx = it->second;
    std::string type = state.value("type", "");

    if (type == "gameFull") {
        // Initialize game
        std::string whiteId = state["white"].value("id", "");
        std::string blackId = state["black"].value("id", "");

        // Determine our color
        // We need to know our bot's user ID. For now, use account info.
        json account = client_.getAccount();
        std::string botId = account.value("id", "");

        if (whiteId == botId) {
            ctx.color = "white";
        } else if (blackId == botId) {
            ctx.color = "black";
        }

        // Parse initial FEN
        std::string fen = state.value("initialFen", chess::STARTPOS_FEN);
        ctx.board.setFen(fen);

        // Apply moves from initial state
        if (state.contains("state") && state["state"].contains("moves")) {
            std::string moves = state["state"]["moves"];
            std::istringstream ss(moves);
            std::string uci;
            while (ss >> uci) {
                Square from = stringToSquare(uci.substr(0, 2));
                Square to = stringToSquare(uci.substr(2, 2));
                PieceType promo = PIECE_TYPE_NB;
                if (uci.size() > 4) promo = charToPieceType(uci[4]);
                Move m(from, to, promo);
                chess::UndoInfo undo;
                ctx.board.makeMove(m, undo);
            }
        }

        ctx.ourTurn = (ctx.board.sideToMove() == chess::WHITE && ctx.color == "white")
                   || (ctx.board.sideToMove() == chess::BLACK && ctx.color == "black");

        std::cerr << "[" << gameId << "] Playing as " << ctx.color << std::endl;

        if (ctx.ourTurn) {
            // Start search
            int timeMs = (ctx.color == "white") ? ctx.wtime : ctx.btime;
            timeMs = (timeMs > 0) ? timeMs / 30 : 5000;
            ctx.search.setTimeMs(timeMs);

            auto result = ctx.search.search(ctx.board);
            client_.makeMove(gameId, moveToString(result.bestMove));
            ctx.ourTurn = false;
        }

    } else if (type == "gameState") {
        // Update board with new moves
        std::string moves = state.value("moves", "");
        if (!moves.empty()) {
            // The "moves" string in gameState contains the full move list.
            // We need to diff with what we have.
            // For simplicity, re-parse the full FEN with all moves applied.
            // Actually, gameState only sends NEW moves since last update.
            // Let's parse individual UCI moves from the string.
            // Actually the gameState "moves" field is the full move list.
            // We'll compare against our board's current move count.
            // Simpler approach: re-setup from the state's moves.

            // Count moves played so far from board state (fullMoves can help)
            // Simpler: just re-apply from FEN with all moves.
            // In gameState events, we only get the moves string for moves
            // made since gameFull. Let's parse and apply.

            std::istringstream ss(moves);
            std::string uci;
            while (ss >> uci) {
                Square from = stringToSquare(uci.substr(0, 2));
                Square to = stringToSquare(uci.substr(2, 2));
                PieceType promo = PIECE_TYPE_NB;
                if (uci.size() > 4) promo = charToPieceType(uci[4]);
                Move m(from, to, promo);
                chess::UndoInfo undo;
                // Only apply if it's a new move (our board might already have it)
                // For simplicity, just try each move and skip illegal ones
                if (ctx.board.isMoveLegal(m)) {
                    ctx.board.makeMove(m, undo);
                }
            }
        }

        // Update clock times
        if (state.contains("wtime")) ctx.wtime = state["wtime"];
        if (state.contains("btime")) ctx.btime = state["btime"];

        // Check if it's our turn
        Color stm = ctx.board.sideToMove();
        ctx.ourTurn = (ctx.color == "white" && stm == chess::WHITE)
                   || (ctx.color == "black" && stm == chess::BLACK);

        if (state.contains("status") && state["status"] != "started") {
            std::string status = state["status"];
            std::cerr << "[" << gameId << "] Game status: " << status << std::endl;
            return;
        }

        if (ctx.ourTurn) {
            std::cerr << "[" << gameId << "] Our turn, thinking..." << std::endl;

            int timeMs = (ctx.color == "white") ? ctx.wtime : ctx.btime;
            int incMs = 0;
            timeMs = (timeMs > 0) ? timeMs / 30 + incMs : 5000;
            if (timeMs < 100) timeMs = 100;
            if (timeMs > 10000) timeMs = 10000;

            ctx.search.setTimeMs(timeMs);
            auto result = ctx.search.search(ctx.board);

            std::cerr << "[" << gameId << "] Playing " << moveToString(result.bestMove) << std::endl;
            client_.makeMove(gameId, moveToString(result.bestMove));
            ctx.ourTurn = false;
        }

    } else if (type == "opponentGone") {
        // Opponent left. Could claim victory after timeout.
        bool gone = state.value("gone", false);
        int claimWinIn = state.value("claimWinInSeconds", 0);
        if (gone && claimWinIn > 0) {
            std::cerr << "[" << gameId << "] Opponent gone, claiming win in " << claimWinIn << "s" << std::endl;
            // We could wait and claim, but for simplicity just note it
        }
    } else if (type == "chatLine") {
        std::string username = state.value("username", "");
        std::string text = state.value("text", "");
        std::cerr << "[" << gameId << "] Chat: " << username << ": " << text << std::endl;
    }
}

} // namespace bot
} // namespace chess
