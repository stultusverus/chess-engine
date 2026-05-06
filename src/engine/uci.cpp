#include "uci.h"
#include "board.h"
#include "search.h"
#include "attacks.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace chess {

UCI::UCI() {
    attacks::init();
    Board::initZobrist();
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
        else if (cmd == "quit")     break;
        else if (cmd == "setoption") handleSetOption(line);
    }
}

void UCI::handleUci() {
    std::cout << "id name ChessEngine" << std::endl;
    std::cout << "id author chess-engine" << std::endl;
    std::cout << "option name Hash type spin default 64 min 1 max 4096" << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCI::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCI::handleUciNewGame() {
    search_.clearTT();
    board_ = Board();
}

void UCI::handlePosition(const std::string& line) {
    std::istringstream ss(line);
    std::string cmd, posType;
    ss >> cmd >> posType; // skip "position"

    if (posType == "startpos") {
        board_.setFen(STARTPOS_FEN);
        std::string moves;
        ss >> moves; // "moves"
    } else if (posType == "fen") {
        std::string fen;
        // Read 6 tokens: placement, stm, castle, ep, half, full
        for (int i = 0; i < 6; i++) {
            std::string token;
            ss >> token;
            if (!fen.empty()) fen += ' ';
            fen += token;
        }
        board_.setFen(fen);
    }

    // Parse moves
    std::string token;
    ss >> token; // "moves" or already part of fen
    if (token != "moves" && !token.empty()) {
        // Re-read
    }
    while (ss >> token) {
        if (token == "moves") continue;
        Square from = stringToSquare(token.substr(0, 2));
        Square to = stringToSquare(token.substr(2, 2));
        PieceType promo = PIECE_TYPE_NB;
        if (token.size() > 4)
            promo = charToPieceType(token[4]);

        Move m(from, to, promo);
        UndoInfo undo;
        board_.makeMove(m, undo);
    }
}

void UCI::handleGo(const std::string& line) {
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
        if (timeMs < 10) timeMs = 10;
    } else if (depth > 0) {
        timeMs = 999999; // Effectively infinite for depth mode
    } else {
        timeMs = 3000; // Default
    }

    search_.setTimeMs(timeMs);

    int maxDepth = depth > 0 ? depth : 64;
    SearchResult result = search_.search(board_, maxDepth);

    std::cout << "bestmove " << moveToString(result.bestMove) << std::endl;
}

void UCI::handleStop() {
    search_.stop();
}

void UCI::handleSetOption(const std::string& line) {
    std::istringstream ss(line);
    std::string cmd, token;
    ss >> cmd; // "setoption"

    std::string name, value;
    while (ss >> token) {
        if (token == "name") {
            while (ss >> token && token != "value")
                name += (name.empty() ? "" : " ") + token;
        }
        if (token == "value") {
            ss >> value;
        }
    }

    if (name == "Hash") {
        search_.setTTSize(std::stoi(value));
    }
}

} // namespace chess
