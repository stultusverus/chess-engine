#include "uci.h"
#include "board.h"
#include "search.h"
#include "attacks.h"
#include "engine/version.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
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

bool isGoValueKeyword(const std::string& token) {
    return token == "wtime" || token == "btime" || token == "winc" ||
           token == "binc" || token == "movestogo" || token == "depth" ||
           token == "movetime" || token == "nodes" || token == "mate";
}

bool isGoFlagKeyword(const std::string& token) {
    return token == "infinite" || token == "ponder" || token == "searchmoves";
}

bool isGoKeyword(const std::string& token) {
    return isGoValueKeyword(token) || isGoFlagKeyword(token);
}

bool rootMoveMatches(Move allowed, Move move) {
    return allowed.from == move.from && allowed.to == move.to &&
           (allowed.promotion == PIECE_TYPE_NB || allowed.promotion == move.promotion);
}

bool moveAllowedByRootMoves(Move move, const std::vector<Move>& rootMoves) {
    if (rootMoves.empty()) return true;
    for (Move allowed : rootMoves) {
        if (rootMoveMatches(allowed, move))
            return true;
    }
    return false;
}

std::optional<Move> firstAllowedLegalMove(Board board, const std::vector<Move>& rootMoves) {
    MoveList legalMoves;
    MoveGenerator gen;
    gen.generateLegalMoves(board, legalMoves);

    for (Move move : legalMoves) {
        if (moveAllowedByRootMoves(move, rootMoves))
            return move;
    }
    return std::nullopt;
}

void emitSearchInfo(const SearchResult& result, bool showWdl, int multiPvIndex = 0) {
    std::cout << "info depth " << result.depth;
    if (multiPvIndex > 0)
        std::cout << " multipv " << multiPvIndex;

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

    std::cout << " nodes " << result.nodes
              << " time " << result.timeMs
              << " nps " << result.nps
              << " hashfull " << result.hashFull;
    if (!result.pv.empty()) {
        std::cout << " pv";
        for (Move move : result.pv)
            std::cout << ' ' << moveToString(move);
    } else if (result.bestMove.from != SQ_NONE && result.bestMove.to != SQ_NONE) {
        std::cout << " pv " << moveToString(result.bestMove);
    }
    std::cout << std::endl;
}

} // namespace

static constexpr int MIN_CLOCK_OVERHEAD_MS = 50;

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
        else if (cmd == "ponderhit") handlePonderHit();
        else if (cmd == "quit")     { stopSearch(); break; }
        else if (cmd == "setoption") handleSetOption(line);
    }
}

void UCI::handleUci() {
    std::cout << "id name ChessEngine " << CHESS_ENGINE_VERSION << std::endl;
    std::cout << "id author chess-engine" << std::endl;
    std::cout << "option name Hash type spin default 64 min 1 max 4096" << std::endl;
    std::cout << "option name Move Overhead type spin default 50 min 0 max 5000" << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 4" << std::endl;
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
            if (!(ss >> token)) {
                std::cerr << "[uci] illegal fen: " << fen << std::endl;
                return;
            }
            if (!fen.empty()) fen += ' ';
            fen += token;
        }
        if (!board_.setFen(fen)) {
            std::cerr << "[uci] illegal fen: " << fen << std::endl;
            return;
        }
    } else {
        std::cerr << "[uci] illegal position: " << posType << std::endl;
        return;
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
    int movestogo = 0, depth = 0, movetime = 0, mate = 0;
    uint64_t nodeLimit = 0;
    bool infinite = false;
    bool unsupportedPonder = false;
    std::vector<Move> searchMoves;

    std::vector<std::string> tokens;
    while (ss >> token)
        tokens.push_back(token);

    auto readValue = [&](size_t& i, const std::string& name, int& target) {
        if (i + 1 >= tokens.size()) {
            std::cerr << "[uci] illegal go " << name << ": missing value" << std::endl;
            return;
        }
        auto value = parseInt(tokens[++i]);
        if (!value) {
            std::cerr << "[uci] illegal go " << name << ": " << tokens[i] << std::endl;
            return;
        }
        target = *value;
    };

    auto readUnsignedValue = [&](size_t& i, const std::string& name, uint64_t& target) {
        if (i + 1 >= tokens.size()) {
            std::cerr << "[uci] illegal go " << name << ": missing value" << std::endl;
            return;
        }
        auto value = parseInt(tokens[++i]);
        if (!value || *value < 0) {
            std::cerr << "[uci] illegal go " << name << ": " << tokens[i] << std::endl;
            return;
        }
        target = static_cast<uint64_t>(*value);
    };

    for (size_t i = 0; i < tokens.size(); i++) {
        token = tokens[i];
        if (token == "wtime") readValue(i, token, wtime);
        else if (token == "btime") readValue(i, token, btime);
        else if (token == "winc") readValue(i, token, winc);
        else if (token == "binc") readValue(i, token, binc);
        else if (token == "movestogo") readValue(i, token, movestogo);
        else if (token == "depth") readValue(i, token, depth);
        else if (token == "movetime") readValue(i, token, movetime);
        else if (token == "mate") readValue(i, token, mate);
        else if (token == "nodes") readUnsignedValue(i, token, nodeLimit);
        else if (token == "infinite") infinite = true;
        else if (token == "ponder") {
            unsupportedPonder = true;
            std::cerr << "[uci] unsupported go token: ponder" << std::endl;
        }
        else if (token == "searchmoves") {
            while (i + 1 < tokens.size() && !isGoKeyword(tokens[i + 1])) {
                std::string moveToken = tokens[++i];
                if (moveToken.size() != 4 && moveToken.size() != 5) {
                    std::cerr << "[uci] illegal go searchmoves: " << moveToken << std::endl;
                    continue;
                }
                Square from = stringToSquare(moveToken.substr(0, 2));
                Square to = stringToSquare(moveToken.substr(2, 2));
                if (from == SQ_NONE || to == SQ_NONE) {
                    std::cerr << "[uci] illegal go searchmoves: " << moveToken << std::endl;
                    continue;
                }
                PieceType promo = PIECE_TYPE_NB;
                if (moveToken.size() == 5) {
                    promo = charToPieceType(moveToken[4]);
                    if (promo == PIECE_TYPE_NB) {
                        std::cerr << "[uci] illegal go searchmoves: " << moveToken << std::endl;
                        continue;
                    }
                }
                searchMoves.emplace_back(from, to, promo);
            }
        } else {
            std::cerr << "[uci] unsupported go token: " << token << std::endl;
        }
    }

    if (depth < 0) depth = 0;
    if (mate < 0) mate = 0;
    if (movestogo < 0) movestogo = 0;
    wtime = std::max(0, wtime);
    btime = std::max(0, btime);
    winc = std::max(0, winc);
    binc = std::max(0, binc);
    movetime = std::max(0, movetime);

    if (unsupportedPonder) {
        std::cout << "info string unsupported ponder" << std::endl;
        std::cout << "bestmove 0000" << std::endl;
        return;
    }

    // Calculate time
    int softTimeMs = 0;
    int hardTimeMs = 0;
    int effectiveOverheadMs = 0;
    int availableTime = 0;
    bool clockManagedSearch = false;
    bool moveImmediately = false;
    if (wtime > 0 || btime > 0) {
        availableTime = (board_.sideToMove() == WHITE) ? wtime : btime;
    }

    if (nodeLimit > 0) {
        search_.setNodeLimit(nodeLimit);
    } else if (infinite) {
        search_.setInfinite(true);
    } else if (movetime >= 0 && std::find(tokens.begin(), tokens.end(), "movetime") != tokens.end()) {
        softTimeMs = std::max(1, movetime);
        hardTimeMs = softTimeMs;
    } else if (wtime > 0 || btime > 0) {
        int myTime = (board_.sideToMove() == WHITE) ? wtime : btime;
        int myInc = (board_.sideToMove() == WHITE) ? winc : binc;
        if (movestogo <= 0) movestogo = 30;
        clockManagedSearch = true;
        effectiveOverheadMs = std::max(moveOverheadMs_, MIN_CLOCK_OVERHEAD_MS);
        if (myTime <= effectiveOverheadMs) {
            moveImmediately = true;
        }
        softTimeMs = myTime / movestogo + myInc;
        softTimeMs = std::min(softTimeMs, myTime / 2);
        hardTimeMs = std::max(softTimeMs, std::min(myTime, std::max(softTimeMs * 3, softTimeMs + myInc + 50)));
    } else if (depth > 0 || mate > 0) {
        search_.setInfinite(true);
        infinite = true;
    } else {
        softTimeMs = 3000; // Default
        hardTimeMs = softTimeMs;
    }

    if (clockManagedSearch && nodeLimit == 0 && !infinite && !moveImmediately &&
        softTimeMs > 0 && effectiveOverheadMs > 0) {
        softTimeMs = std::max(0, softTimeMs - effectiveOverheadMs);
        hardTimeMs = std::max(0, hardTimeMs - effectiveOverheadMs);
    }
    if (nodeLimit == 0 && !infinite && !moveImmediately && availableTime > 0) {
        int hardCap = std::max(0, availableTime - effectiveOverheadMs);
        if (clockManagedSearch && hardCap <= 0)
            moveImmediately = true;
        hardTimeMs = std::min(hardTimeMs, hardCap);
        softTimeMs = std::min(softTimeMs, hardTimeMs);
    }
    if (nodeLimit == 0 && !infinite && !moveImmediately && softTimeMs <= 0) {
        softTimeMs = 1;
    }
    if (nodeLimit == 0 && !infinite && !moveImmediately) {
        hardTimeMs = std::max(softTimeMs, hardTimeMs);
        search_.setTimeControlMs(softTimeMs, hardTimeMs);
    }

    // Probe opening book
    if (bookEnabled_ && book_.isLoaded() && !infinite && !unsupportedPonder && multiPv_ <= 1) {
        auto bookMove = book_.probe(board_);
        if (bookMove && !moveAllowedByRootMoves(*bookMove, searchMoves)) {
            bookMove.reset();
        }
        if (bookMove) {
            Board bookBoard = board_;
            UndoInfo undo;
            if (!bookBoard.makeMove(*bookMove, undo)) {
                bookMove.reset();
            }
        }
        if (bookMove) {
            std::cerr << "info string book move" << std::endl;
            std::cout << "bestmove " << moveToString(*bookMove) << std::endl;
            return;
        }
    }

    if (moveImmediately) {
        auto move = firstAllowedLegalMove(board_, searchMoves);
        if (move) {
            std::cout << "bestmove " << moveToString(*move) << std::endl;
        } else {
            std::cout << "bestmove 0000" << std::endl;
        }
        return;
    }

    if (mate > 0 && depth <= 0)
        depth = mate * 2 - 1;
    int maxDepth = (infinite && depth <= 0) ? MAX_PLY : (depth > 0 ? depth : 64);
    Board searchBoard = board_;
    bool showWdl = showWdl_;
    int multiPv = multiPv_;
    pondering_.store(false);
    search_.setRootMoves(searchMoves);
    auto sharedSearchStart = std::chrono::steady_clock::now();
    if (multiPv <= 1) {
        search_.setInfoCallback([showWdl](const SearchResult& result) {
            emitSearchInfo(result, showWdl);
        });
    } else {
        search_.setInfoCallback(nullptr);
    }
    searchRunning_.store(true);
    bool hasRootMoves = !searchMoves.empty();
    searchThread_ = std::thread([this, searchBoard, maxDepth, hasRootMoves, showWdl, multiPv, searchMoves, sharedSearchStart]() mutable {
        SearchResult result{};
        if (multiPv <= 1) {
            result = search_.search(searchBoard, maxDepth);
        } else {
            MoveList legalMoves;
            MoveGenerator gen;
            Board legalBoard = searchBoard;
            gen.generateLegalMoves(legalBoard, legalMoves);

            std::vector<Move> remaining;
            for (const Move& move : legalMoves) {
                if (!hasRootMoves) {
                    remaining.push_back(move);
                } else {
                    for (const Move& allowed : searchMoves) {
                        if (allowed.from == move.from && allowed.to == move.to &&
                            (allowed.promotion == PIECE_TYPE_NB || allowed.promotion == move.promotion)) {
                            remaining.push_back(move);
                            break;
                        }
                    }
                }
            }

            int lines = std::min(multiPv, static_cast<int>(remaining.size()));
            search_.setTimeStartOverride(sharedSearchStart);
            for (int line = 1; line <= lines && !remaining.empty() && !search_.isStopped(); line++) {
                search_.setRootMoves(remaining);
                SearchResult lineResult = search_.search(searchBoard, maxDepth);
                if (lineResult.bestMove.from == SQ_NONE || lineResult.bestMove.to == SQ_NONE)
                    break;
                emitSearchInfo(lineResult, showWdl, line);
                if (line == 1)
                    result = lineResult;

                remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
                    [&](Move move) {
                        return move.from == lineResult.bestMove.from &&
                               move.to == lineResult.bestMove.to &&
                               move.promotion == lineResult.bestMove.promotion;
                    }), remaining.end());
            }
            search_.clearTimeStartOverride();
        }
        if (!hasRootMoves && (result.bestMove.from == SQ_NONE || result.bestMove.to == SQ_NONE)) {
            MoveList moves;
            MoveGenerator gen;
            gen.generateMoves(searchBoard, moves);
            if (moves.size() > 0) {
                result.bestMove = moves[0];
            }
        }

        if (result.bestMove.from == SQ_NONE || result.bestMove.to == SQ_NONE) {
            std::cout << "bestmove 0000" << std::endl;
        } else {
            std::cout << "bestmove " << moveToString(result.bestMove) << std::endl;
        }
        search_.clearRootMoves();
        search_.setInfoCallback(nullptr);
        pondering_.store(false);
        searchRunning_.store(false);
    });
}

void UCI::handleStop() {
    stopSearch();
}

void UCI::handlePonderHit() {
    if (pondering_.load()) {
        pondering_.store(false);
        stopSearch();
    }
}

void UCI::stopSearch() {
    search_.stop();
    if (searchThread_.joinable()) {
        searchThread_.join();
    }
    pondering_.store(false);
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
    } else if (name == "MultiPV") {
        if (auto count = parseInt(value)) {
            multiPv_ = std::clamp(*count, 1, 4);
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
