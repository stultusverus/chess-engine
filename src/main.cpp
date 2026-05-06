#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/eval.h"
#include "engine/movegen.h"
#include "engine/search.h"
#include "engine/types.h"
#include <iostream>

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    chess::Board board;

    // Initial test
    std::cout << "Start position FEN: " << board.fen() << std::endl;

    // Perft quick check
    chess::MoveGenerator gen;
    std::cout << "Perft(1)=" << gen.perft(board, 1) << " Perft(2)=" << gen.perft(board, 2) << std::endl;

    // Eval
    chess::Eval eval;
    std::cout << "Eval: " << eval.evaluate(board) << std::endl;

    // Search
    chess::Search search;
    search.setTimeMs(3000);
    std::cout << "Searching..." << std::endl;
    auto result = search.search(board, 10);
    std::cout << "bestmove " << chess::moveToString(result.bestMove) << std::endl;
    std::cout << "score: " << result.score << " depth: " << result.depth << " nodes: " << result.nodes << std::endl;

    return 0;
}
