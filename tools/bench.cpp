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

struct PerftBenchResult {
    std::string name;
    int depth = 0;
    uint64_t nodes = 0;
};

struct SearchBenchResult {
    int depth = 0;
    uint64_t nodes = 0;
    int timeMs = 0;
    uint64_t nps = 0;
    std::string bestMove;
};

struct EpdBenchResult {
    bool ok = false;
    std::string bestMove;
    std::vector<std::string> expected;
    uint64_t nodes = 0;
};

std::string jsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

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

void printBuiltInJson(const std::vector<PerftBenchResult>& perftResults,
                      uint64_t totalNodes, int64_t perftElapsed, uint64_t perftNps,
                      const SearchBenchResult& searchResult) {
    std::cout << "{\n";
    std::cout << "  \"perft\": [\n";
    for (size_t i = 0; i < perftResults.size(); i++) {
        const PerftBenchResult& result = perftResults[i];
        std::cout << "    {\"name\": \"" << jsonEscape(result.name)
                  << "\", \"depth\": " << result.depth
                  << ", \"nodes\": " << result.nodes << "}";
        if (i + 1 < perftResults.size()) std::cout << ',';
        std::cout << '\n';
    }
    std::cout << "  ],\n";
    std::cout << "  \"perftTotal\": {\"nodes\": " << totalNodes
              << ", \"timeMs\": " << perftElapsed
              << ", \"nps\": " << perftNps << "},\n";
    std::cout << "  \"search\": {\"depth\": " << searchResult.depth
              << ", \"nodes\": " << searchResult.nodes
              << ", \"timeMs\": " << searchResult.timeMs
              << ", \"nps\": " << searchResult.nps
              << ", \"bestmove\": \"" << jsonEscape(searchResult.bestMove) << "\"}\n";
    std::cout << "}\n";
}

void runBuiltInBench(bool json) {
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
    std::vector<PerftBenchResult> perftResults;
    uint64_t totalNodes = 0;
    auto start = std::chrono::steady_clock::now();

    for (const Position& pos : positions) {
        chess::Board board(pos.fen);
        uint64_t nodes = gen.perft(board, 4);
        totalNodes += nodes;
        perftResults.push_back({pos.name, 4, nodes});
        if (!json)
            std::cout << "perft name " << pos.name << " depth 4 nodes " << nodes << '\n';
    }

    auto perftElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t perftNps = perftElapsed > 0 ? totalNodes * 1000ULL / static_cast<uint64_t>(perftElapsed) : totalNodes;
    if (!json) {
        std::cout << "perft-total nodes " << totalNodes << " time " << perftElapsed
                  << " nps " << perftNps << '\n';
    }

    chess::Search search;
    search.setInfinite(true);
    chess::Board board(chess::STARTPOS_FEN);
    auto result = search.search(board, 6);
    SearchBenchResult searchResult{
        result.depth,
        result.nodes,
        result.timeMs,
        result.nps,
        chess::moveToString(result.bestMove),
    };

    if (json) {
        printBuiltInJson(perftResults, totalNodes, perftElapsed, perftNps, searchResult);
    } else {
        std::cout << "search depth " << searchResult.depth
                  << " nodes " << searchResult.nodes
                  << " time " << searchResult.timeMs
                  << " nps " << searchResult.nps
                  << " bestmove " << searchResult.bestMove << '\n';
    }
}

void printEpdJson(int solved, int total, const std::vector<EpdBenchResult>& results) {
    std::cout << "{\n";
    std::cout << "  \"summary\": {\"solved\": " << solved << ", \"total\": " << total << "},\n";
    std::cout << "  \"positions\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const EpdBenchResult& result = results[i];
        std::cout << "    {\"ok\": " << (result.ok ? "true" : "false")
                  << ", \"bestmove\": \"" << jsonEscape(result.bestMove)
                  << "\", \"expected\": [";
        for (size_t j = 0; j < result.expected.size(); j++) {
            std::cout << "\"" << jsonEscape(result.expected[j]) << "\"";
            if (j + 1 < result.expected.size()) std::cout << ", ";
        }
        std::cout << "], \"nodes\": " << result.nodes << "}";
        if (i + 1 < results.size()) std::cout << ',';
        std::cout << '\n';
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}

int runEpd(const std::string& path, bool json) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "error: cannot open EPD file: " << path << '\n';
        return 1;
    }

    int total = 0;
    int solved = 0;
    std::vector<EpdBenchResult> results;
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
        results.push_back({ok, bestMove, bestMoves, result.nodes});

        if (!json) {
            std::cout << (ok ? "ok" : "miss")
                      << " bestmove " << bestMove
                      << " expected";
            for (const std::string& move : bestMoves)
                std::cout << ' ' << move;
            std::cout << " nodes " << result.nodes << '\n';
        }
    }

    if (json)
        printEpdJson(solved, total, results);
    else
        std::cout << "epd solved " << solved << " total " << total << '\n';
    return total == 0 ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    chess::attacks::init();
    chess::Board::initZobrist();

    bool json = false;
    std::string epdPath;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--json") {
            json = true;
        } else {
            epdPath = arg;
        }
    }

    if (!epdPath.empty())
        return runEpd(epdPath, json);

    runBuiltInBench(json);
    return 0;
}
