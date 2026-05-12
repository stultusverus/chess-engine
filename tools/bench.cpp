#include "engine/attacks.h"
#include "engine/board.h"
#include "engine/movegen.h"
#include "engine/search.h"
#include "engine/types.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string firstFenFields(const std::string& line) {
    std::istringstream ss(line);
    std::string field;
    std::vector<std::string> fields;
    while (fields.size() < 4 && ss >> field)
        fields.push_back(field);

    if (fields.size() != 4)
        return "";

    return fields[0] + " " + fields[1] + " " + fields[2] + " " + fields[3] + " 0 1";
}

std::vector<std::string> bestMovesFromEpd(const std::string& line) {
    std::vector<std::string> moves;
    std::size_t bm = line.find(" bm ");
    if (bm == std::string::npos)
        bm = line.find("bm ");
    if (bm == std::string::npos)
        return moves;

    std::size_t start = line.find("bm", bm);
    if (start == std::string::npos)
        return moves;
    start += 2;

    std::size_t end = line.find(';', start);
    std::string moveList = line.substr(start, end == std::string::npos ? std::string::npos : end - start);
    std::istringstream ss(moveList);
    std::string move;
    while (ss >> move)
        moves.push_back(move);
    return moves;
}

bool containsMove(const std::vector<std::string>& moves, const std::string& move) {
    for (const std::string& candidate : moves) {
        if (candidate == move)
            return true;
    }
    return false;
}

void runBuiltInBench() {
    struct Position {
        const char* name;
        const char* fen;
    };

    const Position positions[] = {
        {"startpos", chess::STARTPOS_FEN},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
        {"middlegame", "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1"},
    };

    chess::MoveGenerator gen;
    uint64_t totalNodes = 0;
    auto start = std::chrono::steady_clock::now();

    for (const Position& pos : positions) {
        chess::Board board(pos.fen);
        uint64_t nodes = gen.perft(board, 4);
        totalNodes += nodes;
        std::cout << "perft name " << pos.name << " depth 4 nodes " << nodes << '\n';
    }

    auto perftElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t perftNps = perftElapsed > 0 ? totalNodes * 1000ULL / static_cast<uint64_t>(perftElapsed) : totalNodes;
    std::cout << "perft-total nodes " << totalNodes << " time " << perftElapsed
              << " nps " << perftNps << '\n';

    chess::Search search;
    search.setInfinite(true);
    chess::Board board(chess::STARTPOS_FEN);
    auto result = search.search(board, 6);
    std::cout << "search depth " << result.depth << " nodes " << result.nodes
              << " time " << result.timeMs << " nps " << result.nps
              << " bestmove " << chess::moveToString(result.bestMove) << '\n';
}

int runEpd(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "error: cannot open EPD file: " << path << '\n';
        return 1;
    }

    int total = 0;
    int solved = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::string fen = firstFenFields(line);
        std::vector<std::string> bestMoves = bestMovesFromEpd(line);
        if (fen.empty() || bestMoves.empty())
            continue;

        chess::Board board(fen);
        chess::Search search;
        search.setInfinite(true);
        auto result = search.search(board, 5);
        std::string bestMove = chess::moveToString(result.bestMove);
        bool ok = containsMove(bestMoves, bestMove);
        solved += ok ? 1 : 0;
        total++;

        std::cout << (ok ? "ok" : "miss")
                  << " bestmove " << bestMove
                  << " expected";
        for (const std::string& move : bestMoves)
            std::cout << ' ' << move;
        std::cout << " nodes " << result.nodes << '\n';
    }

    std::cout << "epd solved " << solved << " total " << total << '\n';
    return total == 0 ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    chess::attacks::init();
    chess::Board::initZobrist();

    if (argc > 1)
        return runEpd(argv[1]);

    runBuiltInBench();
    return 0;
}
