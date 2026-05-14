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
    bool skipped = false;
    std::string fen;
    std::string bestMove;
    std::vector<std::string> expected;
    std::vector<std::string> avoid;
    uint64_t nodes = 0;
    std::string skipReason;
};

struct BenchPositionResult {
    std::string fen;
    std::string bestMove;
    int score = 0;
    int depth = 0;
    uint64_t nodes = 0;
    int timeMs = 0;
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

std::vector<std::string> avoidMovesFromEpd(const std::string& line) {
    std::vector<std::string> moves;
    auto extract = [&](const std::string& tag) {
        std::size_t pos = line.find(" " + tag + " ");
        if (pos == std::string::npos)
            pos = line.find(tag + " ");
        if (pos == std::string::npos)
            return;
        std::size_t start = line.find(tag, pos);
        if (start == std::string::npos)
            return;
        start += tag.size();
        std::size_t end = line.find(';', start);
        std::string moveList = line.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::istringstream ss(moveList);
        std::string move;
        while (ss >> move)
            moves.push_back(move);
    };
    extract("am");
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

void printEpdJson(int solved, int total, int skipped, const std::vector<EpdBenchResult>& results) {
    std::cout << "{\n";
    std::cout << "  \"summary\": {\"solved\": " << solved
              << ", \"total\": " << total
              << ", \"skipped\": " << skipped << "},\n";
    std::cout << "  \"positions\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const EpdBenchResult& result = results[i];
        std::cout << "    {\"fen\": \"" << jsonEscape(result.fen) << "\"";
        if (result.skipped) {
            std::cout << ", \"skipped\": true"
                      << ", \"reason\": \"" << jsonEscape(result.skipReason) << "\"";
        } else {
            std::cout << ", \"ok\": " << (result.ok ? "true" : "false")
                      << ", \"bestmove\": \"" << jsonEscape(result.bestMove) << "\"";
            if (!result.expected.empty()) {
                std::cout << ", \"expected\": [";
                for (size_t j = 0; j < result.expected.size(); j++) {
                    std::cout << "\"" << jsonEscape(result.expected[j]) << "\"";
                    if (j + 1 < result.expected.size()) std::cout << ", ";
                }
                std::cout << "]";
            }
            if (!result.avoid.empty()) {
                std::cout << ", \"avoid\": [";
                for (size_t j = 0; j < result.avoid.size(); j++) {
                    std::cout << "\"" << jsonEscape(result.avoid[j]) << "\"";
                    if (j + 1 < result.avoid.size()) std::cout << ", ";
                }
                std::cout << "]";
            }
            std::cout << ", \"nodes\": " << result.nodes;
        }
        std::cout << "}";
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
    int skipped = 0;
    std::vector<EpdBenchResult> results;
    std::string line;
    int lineNo = 0;
    while (std::getline(file, line)) {
        lineNo++;
        if (line.empty() || line[0] == '#')
            continue;

        std::string fen = firstFenFields(line);
        std::vector<std::string> bestMoves = bestMovesFromEpd(line);
        std::vector<std::string> avoidMoves = avoidMovesFromEpd(line);

        // Malformed or unsupported: report and skip
        if (fen.empty()) {
            EpdBenchResult skippedResult;
            skippedResult.skipped = true;
            skippedResult.fen = line;
            skippedResult.skipReason = "invalid FEN at line " + std::to_string(lineNo);
            results.push_back(skippedResult);
            skipped++;
            if (!json)
                std::cerr << "skip line " << lineNo << ": invalid FEN\n";
            continue;
        }

        if (bestMoves.empty() && avoidMoves.empty()) {
            EpdBenchResult skippedResult;
            skippedResult.skipped = true;
            skippedResult.fen = fen;
            skippedResult.skipReason = "no bm or am operation at line " + std::to_string(lineNo);
            results.push_back(skippedResult);
            skipped++;
            if (!json)
                std::cerr << "skip line " << lineNo << ": no bm or am operation\n";
            continue;
        }

        chess::Board board(fen);
        chess::Search search;
        search.setInfinite(true);
        auto searchResult = search.search(board, 5);
        std::string bestMove = chess::moveToString(searchResult.bestMove);

        bool ok;
        if (!avoidMoves.empty() && bestMoves.empty()) {
            // am-only: pass if bestmove is NOT in the avoid list
            ok = !containsMove(avoidMoves, bestMove);
        } else if (!bestMoves.empty() && !avoidMoves.empty()) {
            // both bm and am: pass if in bm list AND not in am list
            ok = containsMove(bestMoves, bestMove) && !containsMove(avoidMoves, bestMove);
        } else {
            // bm-only: pass if bestmove is in the bm list
            ok = containsMove(bestMoves, bestMove);
        }

        solved += ok ? 1 : 0;
        total++;

        EpdBenchResult benchResult;
        benchResult.ok = ok;
        benchResult.fen = fen;
        benchResult.bestMove = bestMove;
        benchResult.expected = bestMoves;
        benchResult.avoid = avoidMoves;
        benchResult.nodes = searchResult.nodes;
        results.push_back(benchResult);

        if (!json) {
            std::cout << (ok ? "ok" : "miss")
                      << " fen " << fen
                      << " bestmove " << bestMove;
            if (!bestMoves.empty()) {
                std::cout << " expected";
                for (const std::string& move : bestMoves)
                    std::cout << ' ' << move;
            }
            if (!avoidMoves.empty()) {
                std::cout << " avoid";
                for (const std::string& move : avoidMoves)
                    std::cout << ' ' << move;
            }
            std::cout << " nodes " << searchResult.nodes << '\n';
        }
    }

    if (json)
        printEpdJson(solved, total, skipped, results);
    else
        std::cout << "epd solved " << solved << " total " << total
                  << " skipped " << skipped << '\n';
    if (total == 0)
        return 1;
    return (solved == total && skipped == 0) ? 0 : 1;
}

// --- Bench signature mode ---

static const char* benchFens[] = {
    // Opening
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    // Open Sicilian
    "r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    // French Defence
    "rnbqkbnr/ppp2ppp/4p3/3pP3/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 0 3",
    // Ruy Lopez
    "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 3",
    // Queen's Gambit
    "rnbqkbnr/ppp2ppp/4p3/3p4/2PP4/8/PP2PPPP/RNBQKBNR w KQkq d6 0 3",
    // KIWIPETE (tactical)
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    // Middlegame position
    "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1",
    // Open position
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    // Endgame with pawns
    "4k3/ppp5/8/8/8/8/PPP5/4K3 w - - 0 1",
    // Rook endgame
    "4k3/pppppppp/8/8/8/8/PPPPPPPP/4K2R w K - 0 1",
    // Bishop vs knight endgame
    "6k1/8/4b3/8/8/4N3/8/6K1 w - - 0 1",
    // Complex middlegame
    "r3k2r/pppq1ppp/2npbn2/4p3/2B1P3/2NPBN2/PPPQ1PPP/R3K2R w KQkq - 4 8",
    // King's Indian style
    "rnbq1rk1/ppp1ppbp/3p1np1/8/2PPP3/2N2N2/PP3PPP/R1BQKB1R w KQ - 4 7",
    // Opposite side castling
    "r3kb1r/pp1n1ppp/2p1pn2/q2p4/2PP4/2N1PN2/PPQ2PPP/R1B1K2R b KQkq - 2 6",
    // Pawn storm endgame
    "8/5p2/4k3/8/8/4K3/5P2/8 w - - 0 1",
};

static constexpr int BENCH_DEPTH = 8;

static uint64_t fnv64(const uint8_t* data, size_t len, uint64_t hash) {
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint64_t computeBenchSignature(const std::vector<BenchPositionResult>& results) {
    uint64_t hash = 14695981039346656037ULL; // FNV-64 offset basis
    for (const auto& r : results) {
        // Hash decimal text of nodes (endian-independent) + bestmove string.
        // timeMs and NPS are excluded — they vary across runs.
        std::string nodeStr = std::to_string(r.nodes);
        hash = fnv64(reinterpret_cast<const uint8_t*>(nodeStr.data()), nodeStr.size(), hash);
        hash = fnv64(reinterpret_cast<const uint8_t*>(r.bestMove.data()), r.bestMove.size(), hash);
    }
    return hash;
}

static void printBenchJson(const std::vector<BenchPositionResult>& results,
                           uint64_t totalNodes, int64_t totalTime, uint64_t nps,
                           uint64_t signature) {
    std::cout << "{\n";
    std::cout << "  \"positions\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        std::cout << "    {\"fen\": \"" << jsonEscape(r.fen)
                  << "\", \"bestmove\": \"" << jsonEscape(r.bestMove)
                  << "\", \"score\": " << r.score
                  << ", \"depth\": " << r.depth
                  << ", \"nodes\": " << r.nodes
                  << ", \"timeMs\": " << r.timeMs << "}";
        if (i + 1 < results.size()) std::cout << ',';
        std::cout << '\n';
    }
    std::cout << "  ],\n";
    std::cout << "  \"total\": {\"nodes\": " << totalNodes
              << ", \"timeMs\": " << totalTime
              << ", \"nps\": " << nps
              << ", \"signature\": \"" << std::hex << signature << std::dec << "\"}\n";
    std::cout << "}\n";
}

int runBench(bool json) {
    std::vector<BenchPositionResult> results;
    uint64_t totalNodes = 0;
    auto benchStart = std::chrono::steady_clock::now();

    for (const char* fen : benchFens) {
        chess::Board board(fen);
        chess::Search search;
        search.clearTT();
        search.setInfinite(true);

        auto posStart = std::chrono::steady_clock::now();
        auto result = search.search(board, BENCH_DEPTH);
        auto posEnd = std::chrono::steady_clock::now();
        int posTime = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(posEnd - posStart).count());

        BenchPositionResult bpr;
        bpr.fen = fen;
        bpr.bestMove = chess::moveToString(result.bestMove);
        bpr.score = result.score;
        bpr.depth = result.depth;
        bpr.nodes = result.nodes;
        bpr.timeMs = posTime;

        results.push_back(bpr);
        totalNodes += result.nodes;

        if (!json) {
            std::cout << "bench fen " << fen
                      << " bestmove " << bpr.bestMove
                      << " score " << bpr.score
                      << " depth " << bpr.depth
                      << " nodes " << bpr.nodes
                      << " time " << bpr.timeMs << '\n';
        }
    }

    auto benchEnd = std::chrono::steady_clock::now();
    int64_t totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStart).count();
    uint64_t nps = totalTime > 0 ? totalNodes * 1000ULL / static_cast<uint64_t>(totalTime) : totalNodes;
    uint64_t signature = computeBenchSignature(results);

    if (json) {
        printBenchJson(results, totalNodes, totalTime, nps, signature);
    } else {
        std::cout << "bench-total nodes " << totalNodes
                  << " time " << totalTime
                  << " nps " << nps
                  << " signature " << std::hex << signature << std::dec << '\n';
    }

    return 0;
}

} // namespace

int runEvalTrace(const std::string& fen, bool json) {
    chess::Board board;
    if (!board.setFen(fen)) {
        std::cerr << "error: invalid FEN: " << fen << '\n';
        return 1;
    }
    chess::Eval eval;
    chess::EvalTrace t = eval.trace(board);

    if (json) {
        std::cout << "{\n";
        std::cout << "  \"fen\": \"" << jsonEscape(fen) << "\",\n";
        std::cout << "  \"phase\": " << t.phase << ",\n";
        std::cout << "  \"terms\": [\n";
        auto entries = t.entries();
        for (size_t i = 0; i < entries.size(); i++) {
            std::cout << "    {\"name\": \"" << jsonEscape(entries[i].name)
                      << "\", \"value\": " << entries[i].value << "}";
            if (i + 1 < entries.size()) std::cout << ',';
            std::cout << '\n';
        }
        std::cout << "  ]\n";
        std::cout << "}\n";
    } else {
        std::cout << "fen " << fen << '\n';
        auto entries = t.entries();
        for (const auto& e : entries)
            std::cout << e.name << ' ' << e.value << '\n';
    }
    return 0;
}

int main(int argc, char** argv) {
    chess::attacks::init();
    chess::Board::initZobrist();

    bool json = false;
    std::string epdPath;
    bool benchMode = false;
    bool traceMode = false;
    std::string traceFen;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--json") {
            json = true;
        } else if (arg == "bench") {
            benchMode = true;
        } else if (arg == "trace") {
            traceMode = true;
            // Collect remaining non-option tokens as the FEN
            std::vector<std::string> fenParts;
            while (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (next == "--json" || next == "bench" || next == "trace") break;
                fenParts.push_back(next);
                i++;
            }
            for (size_t j = 0; j < fenParts.size(); j++) {
                if (j > 0) traceFen += ' ';
                traceFen += fenParts[j];
            }
        } else {
            epdPath = arg;
        }
    }

    if (traceMode) {
        if (traceFen.empty()) {
            std::cerr << "usage: bench_engine trace <FEN> [--json]\n";
            return 1;
        }
        return runEvalTrace(traceFen, json);
    }

    if (benchMode)
        return runBench(json);

    if (!epdPath.empty())
        return runEpd(epdPath, json);

    runBuiltInBench(json);
    return 0;
}
