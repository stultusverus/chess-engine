#include "uci.h"
#include "board.h"
#include "search.h"
#include "attacks.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace chess {
namespace {

struct Wdl {
    int win;
    int draw;
    int loss;
};

std::optional<int> parseInt(const std::string& value) {
    if (value.empty()) return std::nullopt;

    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0') {
        return std::nullopt;
    }

    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

std::optional<bool> parseBool(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    return std::nullopt;
}

Wdl approximateWdl(int score) {
    constexpr int MATE_SCORE_THRESHOLD = 900000;
    if (score >= MATE_SCORE_THRESHOLD) return {1000, 0, 0};
    if (score <= -MATE_SCORE_THRESHOLD) return {0, 0, 1000};

    double cp = std::clamp(static_cast<double>(score), -1000.0, 1000.0);
    double expected = 1.0 / (1.0 + std::exp(-cp / 200.0));
    int draw = static_cast<int>(std::lround(350.0 * std::exp(-std::abs(cp) / 300.0)));
    draw = std::clamp(draw, 0, 1000);

    int decisive = 1000 - draw;
    int win = static_cast<int>(std::lround(decisive * expected));
    win = std::clamp(win, 0, decisive);

    return {win, draw, decisive - win};
}

void emitSearchInfo(const SearchResult& result, bool showWdl) {
    std::cout << "info depth " << result.depth;

    if (std::abs(result.score) > MATE - MAX_PLY) {
        int plies = MATE - std::abs(result.score);
        int mateIn = (plies + 1) / 2;
        if (result.score < 0) mateIn = -mateIn;
        std::cout << " score mate " << mateIn;
    } else {
        std::cout << " score cp " << result.score;
    }

    if (showWdl) {
        Wdl wdl = approximateWdl(result.score);
        std::cout << " wdl " << wdl.win << ' ' << wdl.draw << ' ' << wdl.loss;
    }

    std::cout << " nodes " << result.nodes;
    if (result.bestMove.from != SQ_NONE && result.bestMove.to != SQ_NONE) {
        std::cout << " pv " << moveToString(result.bestMove);
    }
    std::cout << std::endl;
}

} // namespace

UCI::UCI() {
    attacks::init();
    Board::initZobrist();
}

UCI::~UCI() {
    stopSearch();
}

void UCI::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "uci")           handleUci();
        else if (cmd == "isready")  handleIsReady();
        else if (cmd == "ucinewgame") handleUciNewGame();
        else if (cmd == "position") handlePosition(line);
        else if (cmd == "go")       handleGo(line);
        else if (cmd == "stop")     handleStop();
        else if (cmd == "quit")     { stopSearch(); break; }
        else if (cmd == "setoption") handleSetOption(line);
    }
}

void UCI::handleUci() {
    std::cout << "id name ChessEngine" << std::endl;
    std::cout << "id author chess-engine" << std::endl;
    std::cout << "option name Hash type spin default 64 min 1 max 4096" << std::endl;
    std::cout << "option name Move Overhead type spin default 0 min 0 max 5000" << std::endl;
    std::cout << "option name OwnBook type check default false" << std::endl;
    std::cout << "option name Book File type string default <empty>" << std::endl;
    std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCI::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCI::handleUciNewGame() {
    stopSearch();
    search_.clearTT();
    board_ = Board();
}

void UCI::handlePosition(const std::string& line) {
    stopSearch();

    std::istringstream ss(line);
    std::string cmd, posType;
    ss >> cmd >> posType; // skip "position"

    if (posType == "startpos") {
        board_.setFen(STARTPOS_FEN);
        std::string moves;
        ss >> moves; // consume "moves" keyword
    } else if (posType == "fen") {
        std::string fen;
        // Read 6 tokens: placement, stm, castle, ep, half, full
        for (int i = 0; i < 6; i++) {
            std::string token;
            ss >> token;
            if (!fen.empty()) fen += ' ';
            fen += token;
        }
        if (!board_.setFen(fen)) {
            std::cerr << "[uci] illegal fen: " << fen << std::endl;
            return;
        }
    }

    // Parse moves
    std::string token;
    while (ss >> token) {
        if (token == "moves") continue;
        if (token.size() != 4 && token.size() != 5) {
            std::cerr << "[uci] illegal move: " << token << std::endl;
            break;
        }
        Square from = stringToSquare(token.substr(0, 2));
        Square to = stringToSquare(token.substr(2, 2));
        if (from == SQ_NONE || to == SQ_NONE) {
            std::cerr << "[uci] illegal move: " << token << std::endl;
            break;
        }
        PieceType promo = PIECE_TYPE_NB;
        if (token.size() == 5) {
            promo = charToPieceType(token[4]);
            if (promo == PIECE_TYPE_NB) {
                std::cerr << "[uci] illegal move: " << token << std::endl;
                break;
            }
        }

        Move m(from, to, promo);
        UndoInfo undo;
        if (!board_.makeMove(m, undo)) {
            std::cerr << "[uci] illegal move: " << token << std::endl;
            break;
        }
    }
}

void UCI::handleGo(const std::string& line) {
    stopSearch();

    std::istringstream ss(line);
    std::string cmd, token;
    ss >> cmd; // "go"

    int wtime = 0, btime = 0, winc = 0, binc = 0;
    int movestogo = 0, depth = 0, movetime = 0;
    bool infinite = false;

    while (ss >> token) {
        if (token == "wtime")       ss >> wtime;
        else if (token == "btime")  ss >> btime;
        else if (token == "winc")   ss >> winc;
        else if (token == "binc")   ss >> binc;
        else if (token == "movestogo") ss >> movestogo;
        else if (token == "depth")  ss >> depth;
        else if (token == "movetime") ss >> movetime;
        else if (token == "infinite") infinite = true;
    }

    // Calculate time
    int timeMs = 0;
    int availableTime = 0;
    if (wtime > 0 || btime > 0) {
        availableTime = (board_.sideToMove() == WHITE) ? wtime : btime;
    }

    if (infinite) {
        search_.setInfinite(true);
    } else if (movetime > 0) {
        timeMs = movetime;
    } else if (wtime > 0 || btime > 0) {
        int myTime = (board_.sideToMove() == WHITE) ? wtime : btime;
        int myInc = (board_.sideToMove() == WHITE) ? winc : binc;
        if (movestogo <= 0) movestogo = 30;
        timeMs = myTime / movestogo + myInc;
        // Safety margin
        if (timeMs > myTime / 2) timeMs = myTime / 2;
    } else if (depth > 0) {
        timeMs = 999999; // Effectively infinite for depth mode
    } else {
        timeMs = 3000; // Default
    }

    if (timeMs > 0 && moveOverheadMs_ > 0) {
        timeMs = std::max(0, timeMs - moveOverheadMs_);
    }
    if (!infinite && availableTime > 0) {
        int hardCap = std::max(0, availableTime - moveOverheadMs_);
        timeMs = std::min(timeMs, hardCap);
    }
    if (!infinite && timeMs <= 0) {
        timeMs = 1;
    }
    if (!infinite) {
        search_.setTimeMs(timeMs);
    }

    // Probe opening book
    if (bookEnabled_ && book_.isLoaded()) {
        auto bookMove = book_.probe(board_);
        if (bookMove) {
            std::cerr << "info string book move" << std::endl;
            std::cout << "bestmove " << moveToString(*bookMove) << std::endl;
            return;
        }
    }

    int maxDepth = (infinite && depth <= 0) ? MAX_PLY : (depth > 0 ? depth : 64);
    Board searchBoard = board_;
    bool showWdl = showWdl_;
    searchRunning_.store(true);
    searchThread_ = std::thread([this, searchBoard, maxDepth, showWdl]() mutable {
        SearchResult result = search_.search(searchBoard, maxDepth);
        if (result.bestMove.from == SQ_NONE || result.bestMove.to == SQ_NONE) {
            MoveList moves;
            MoveGenerator gen;
            gen.generateMoves(searchBoard, moves);
            if (moves.size() > 0) {
                result.bestMove = moves[0];
            }
        }

        emitSearchInfo(result, showWdl);
        if (result.bestMove.from == SQ_NONE || result.bestMove.to == SQ_NONE) {
            std::cout << "bestmove 0000" << std::endl;
        } else {
            std::cout << "bestmove " << moveToString(result.bestMove) << std::endl;
        }
        searchRunning_.store(false);
    });
}

void UCI::handleStop() {
    stopSearch();
}

void UCI::stopSearch() {
    search_.stop();
    if (searchThread_.joinable()) {
        searchThread_.join();
    }
    searchRunning_.store(false);
}

void UCI::handleSetOption(const std::string& line) {
    std::istringstream ss(line);
    std::string cmd, token;
    ss >> cmd; // "setoption"

    std::string name, value;
    bool readingName = false;
    bool readingValue = false;
    while (ss >> token) {
        if (token == "name") {
            readingName = true;
            readingValue = false;
            continue;
        }
        if (token == "value") {
            readingName = false;
            readingValue = true;
            continue;
        }
        if (readingName) {
            if (!name.empty()) name += ' ';
            name += token;
        }
        if (readingValue) {
            if (!value.empty()) value += ' ';
            value += token;
        }
    }

    if (name == "Hash") {
        if (auto mb = parseInt(value)) {
            search_.setTTSize(std::clamp(*mb, 1, 4096));
        }
    } else if (name == "Move Overhead") {
        if (auto ms = parseInt(value)) {
            moveOverheadMs_ = std::clamp(*ms, 0, 5000);
        }
    } else if (name == "OwnBook") {
        if (auto enabled = parseBool(value)) {
            bookEnabled_ = *enabled;
        }
    } else if (name == "Book File") {
        if (!value.empty() && value != "<empty>")
            book_.load(value);
    } else if (name == "UCI_ShowWDL") {
        if (auto enabled = parseBool(value)) {
            showWdl_ = *enabled;
        }
    }
}

} // namespace chess
