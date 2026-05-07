#include "manager.h"
#include "decision.h"
#include "engine/attacks.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace chess {
namespace bot {

Manager::Manager(const std::string& token) : client_(token) {
    chess::attacks::init();
    chess::Board::initZobrist();
    workerThread_ = std::thread(&Manager::workerLoop, this);
}

Manager::~Manager() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void Manager::setDebug(bool v) {
    debug_ = v;
    client_.setDebug(v);
}

void Manager::setBookPath(const std::string& path) {
    book_.load(path);
}

bool Manager::tryBookMove(const std::string& gameId, GameContext& ctx) {
    auto bookMove = book_.probe(ctx.board);
    if (bookMove) {
        dbg(gameId, "book move=" + moveToString(*bookMove));
        std::string uci = moveToString(*bookMove);
        enqueue([this, gameId, uci]() {
            client_.makeMove(gameId, uci);
        });
        ctx.ourTurn = false;
        return true;
    }
    return false;
}

bool Manager::maybeResignOrDraw(const std::string& gameId, GameContext& ctx, const chess::SearchResult& result) {
    auto d = evaluatePostSearch(
        ctx.drawOffered, ctx.consecutiveDrawishMoves, result.score,
        autoResign_, resignThreshold_,
        autoDraw_, drawEvalThreshold_, drawOfferMoves_);

    ctx.consecutiveDrawishMoves = d.newConsecutiveDrawishMoves;

    switch (d.action) {
    case PostSearchAction::RESIGN:
        std::cerr << "[" << gameId << "] Resigning (eval " << result.score
                  << " < " << resignThreshold_ << ")" << std::endl;
        enqueue([this, gameId]() {
            client_.resignGame(gameId);
        });
        return true;
    case PostSearchAction::ACCEPT_DRAW:
        std::cerr << "[" << gameId << "] Accepting draw (eval " << result.score << ")" << std::endl;
        ctx.drawOffered = false;
        enqueue([this, gameId]() {
            client_.handleDraw(gameId, true);
        });
        return true;
    case PostSearchAction::OFFER_DRAW: {
        std::string uci = moveToString(result.bestMove);
        std::cerr << "[" << gameId << "] Offering draw (eval " << result.score
                  << " within " << drawEvalThreshold_ << " cp for " << drawOfferMoves_ << " moves)" << std::endl;
        enqueue([this, gameId, uci]() {
            client_.makeMove(gameId, uci, true);
        });
        return false;
    }
    default:
        enqueue([this, gameId, uci = moveToString(result.bestMove)]() {
            client_.makeMove(gameId, uci, false);
        });
        return false;
    }
}

void Manager::enqueue(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(actionMutex_);
    actionQueue_.push_back(std::move(task));
}

void Manager::workerLoop() {
    while (running_) {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(actionMutex_);
            if (!actionQueue_.empty()) {
                task = std::move(actionQueue_.front());
                actionQueue_.pop_front();
            }
        }
        if (task) {
            task();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void Manager::challengeOpponent(const std::string& username, int clockLimit, int clockInc, bool rated) {
    std::cerr << "Challenging " << username
              << " (" << clockLimit << "+" << clockInc << (rated ? ", rated" : ", casual") << ")"
              << std::endl;

    json params;
    params["clock.limit"] = clockLimit;
    params["clock.increment"] = clockInc;
    params["rated"] = rated;

    json response = client_.createChallenge(username, params);
    if (debug_) std::cerr << "[challenge] create response: " << response.dump() << std::endl;

    if (response.contains("challenge") && response["challenge"].contains("id")) {
        std::string challengeId = response["challenge"]["id"];
        std::string status = response["challenge"].value("status", "");
        std::cerr << "  Challenge " << challengeId << " status: " << status << std::endl;
    } else if (response.contains("id")) {
        std::string challengeId = response["id"];
        std::string status = response.value("status", "");
        std::cerr << "  Challenge " << challengeId << " status: " << status << std::endl;
    } else {
        std::cerr << "  Challenge response: " << response.dump() << std::endl;
    }
}

void Manager::challengeBots(int count, int clockLimit, int clockInc, bool rated) {
    if (clockLimit <= 0) clockLimit = 180;  // 3 minutes default
    if (clockInc <= 0) clockInc = 2;         // 2 seconds default

    std::cerr << "Fetching online bots..." << std::endl;
    json bots = client_.getOnlineBots(50);
    std::cerr << "  Found " << bots.size() << " online bots" << std::endl;

    // Get our own ID
    json account = client_.getAccount();
    std::string selfId = account.value("id", "");

    if (debug_) {
        std::cerr << "[debug] Self ID: " << selfId << std::endl;
        for (const auto& b : bots) {
            std::cerr << "[debug]   Bot: " << b.value("id", "?")
                      << " (" << b.value("username", "?") << ")" << std::endl;
        }
    }

    int challenged = 0;
    for (const auto& bot : bots) {
        std::string botId = bot.value("id", "");
        std::string botName = bot.value("username", "");

        if (botId == selfId || botName.empty()) continue;

        challengeOpponent(botName, clockLimit, clockInc, rated);
        challenged++;

        if (challenged >= count) break;
    }

    if (challenged == 0) {
        std::cerr << "  No other bots online to challenge." << std::endl;
    }
}

void Manager::run() {
    std::cerr << "Bot manager starting..." << std::endl;
    if (debug_) {
        std::cerr << "[debug] Debug mode enabled" << std::endl;
        std::cerr << "[debug] Challenge policy: rated=" << (acceptRated_ ? "only" : "any")
                  << " time=[" << minTime_ << "," << maxTime_ << "]"
                  << " min_inc=" << minInc_ << std::endl;
    }

    client_.streamEvents([this](const json& ev) {
        if (!running_) return;
        onEvent(ev);
    });
}

void Manager::onEvent(const json& ev) {
    std::string type = ev.value("type", "");
    if (debug_) std::cerr << "[event] type=" << type << std::endl;

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
    std::string variant = challenge.value("variant", json::object()).value("key", "standard");

    if (debug_) {
        std::cerr << "[challenge] raw=" << challenge.dump() << std::endl;
    }

    std::cerr << "Challenge from " << challenger
              << " (rated=" << rated << " time=" << limit << "+" << increment
              << " variant=" << variant << ")" << std::endl;

    // Apply policy
    if (variant != "standard") {
        std::cerr << "  Declining: only standard chess" << std::endl;
        client_.declineChallenge(id, "Only playing standard chess");
        return;
    }
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
        dbg(gameId, "gameFull event received");

        // Initialize game
        std::string whiteId = state["white"].value("id", "");
        std::string blackId = state["black"].value("id", "");

        // Determine our color - match by ID or username
        json account = client_.getAccount();
        std::string botId = account.value("id", "");
        std::string botName = account.value("username", "");

        if (whiteId == botId || whiteId == botName) {
            ctx.color = "white";
        } else if (blackId == botId || blackId == botName) {
            ctx.color = "black";
        }

        // Fallback: check if our name appears in the opponent field
        if (ctx.color.empty()) {
            std::string whiteName = state["white"].value("name", "");
            std::string blackName = state["black"].value("name", "");
            if (whiteName == botName || whiteId == botId) ctx.color = "white";
            else if (blackName == botName || blackId == botId) ctx.color = "black";
        }

        // If still unknown, use the gameStart event color hint
        if (ctx.color.empty()) {
            std::cerr << "[" << gameId << "] WARNING: could not determine color."
                      << " whiteId=" << whiteId << " blackId=" << blackId
                      << " botId=" << botId << " botName=" << botName << std::endl;
            std::cerr << "[" << gameId << "] FEN=" << ctx.board.fen() << std::endl;
            return;
        }

        // Parse initial FEN
        std::string fen = state.value("initialFen", std::string("startpos"));
        if (fen.empty() || fen == "startpos") fen = chess::STARTPOS_FEN;
        ctx.initialFen = fen;
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

        if (debug_) {
            std::cerr << "[" << gameId << "] Playing as " << ctx.color
                      << " (bot=" << botId << " white=" << whiteId << " black=" << blackId << ")"
                      << std::endl;
            std::cerr << "[" << gameId << "] FEN: " << ctx.board.fen() << std::endl;
            std::cerr << "[" << gameId << "] Our turn: " << (ctx.ourTurn ? "yes" : "no") << std::endl;
        } else {
            std::cerr << "[" << gameId << "] Playing as " << ctx.color << std::endl;
        }
        std::cerr << std::flush; // Ensure output before potential blocking search

        if (ctx.ourTurn) {
            int timeMs = (ctx.color == "white") ? ctx.wtime : ctx.btime;
            timeMs = (timeMs > 0) ? timeMs / 30 : 5000;
            std::cerr << "[" << gameId << "] Thinking (time=" << timeMs << "ms)..." << std::endl;
            dbg(gameId, "searching with time=" + std::to_string(timeMs) + "ms");

            if (tryBookMove(gameId, ctx)) return;

            ctx.search.setTimeMs(timeMs);
            auto result = ctx.search.search(ctx.board);
            dbg(gameId, "bestmove=" + moveToString(result.bestMove) + " score=" + std::to_string(result.score)
                + " depth=" + std::to_string(result.depth) + " nodes=" + std::to_string(result.nodes));

            if (maybeResignOrDraw(gameId, ctx, result)) return;
            ctx.ourTurn = false;
        }

    } else if (type == "gameState") {
        // Rebuild board from scratch using the full move list
        // This is robust against partial sync (avoids illegal-move-on-already-played crashes)
        std::string moves = state.value("moves", "");
        if (!moves.empty()) {
            ctx.board.setFen(ctx.initialFen);
            std::istringstream ss(moves);
            std::string uci;
            int appliedCount = 0;
            while (ss >> uci) {
                Square from = stringToSquare(uci.substr(0, 2));
                Square to = stringToSquare(uci.substr(2, 2));
                PieceType promo = PIECE_TYPE_NB;
                if (uci.size() > 4) promo = charToPieceType(uci[4]);
                Move m(from, to, promo);
                chess::UndoInfo undo;
                ctx.board.makeMove(m, undo);
                appliedCount++;
            }
            dbg(gameId, "applied " + std::to_string(appliedCount) + " moves, FEN: " + ctx.board.fen());
        }

        // Update clock times
        if (state.contains("wtime")) ctx.wtime = state["wtime"];
        if (state.contains("btime")) ctx.btime = state["btime"];

        // Detect opponent draw offer
        if (state.contains("wdraw") || state.contains("bdraw")) {
            bool opponentOffered = (ctx.color == "white" && state.value("bdraw", false))
                                || (ctx.color == "black" && state.value("wdraw", false));
            if (opponentOffered && !ctx.drawOffered) {
                ctx.drawOffered = true;
                std::cerr << "[" << gameId << "] Opponent offered draw" << std::endl;
            }
        }

        // Check if it's our turn
        Color stm = ctx.board.sideToMove();
        ctx.ourTurn = (ctx.color == "white" && stm == chess::WHITE)
                   || (ctx.color == "black" && stm == chess::BLACK);

        if (!ctx.ourTurn) {
            ctx.drawOffered = false;
        }

        if (state.contains("status") && state["status"] != "started") {
            std::string status = state["status"];
            std::cerr << "[" << gameId << "] Game status: " << status << std::endl;
            return;
        }

        if (ctx.ourTurn) {
            dbg(gameId, "our turn, wtime=" + std::to_string(ctx.wtime) + " btime=" + std::to_string(ctx.btime));

            if (tryBookMove(gameId, ctx)) return;

            int timeMs = (ctx.color == "white") ? ctx.wtime : ctx.btime;
            int incMs = 0;
            timeMs = (timeMs > 0) ? timeMs / 30 + incMs : 5000;
            if (timeMs < 100) timeMs = 100;
            if (timeMs > 10000) timeMs = 10000;

            dbg(gameId, "searching with time=" + std::to_string(timeMs) + "ms");

            ctx.search.setTimeMs(timeMs);
            auto result = ctx.search.search(ctx.board);

            dbg(gameId, "bestmove=" + moveToString(result.bestMove) + " score=" + std::to_string(result.score)
                + " depth=" + std::to_string(result.depth) + " nodes=" + std::to_string(result.nodes));

            if (maybeResignOrDraw(gameId, ctx, result)) return;
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
