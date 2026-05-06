#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/types.h"
#include <iostream>
#include <cassert>

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    chess::Board board;

    // Verify start position
    assert(board.sideToMove() == chess::WHITE);
    assert(board.enPassant() == chess::SQ_NONE);
    assert(board.castlingRights() == chess::ALL);
    assert(board.pieceOn(chess::E1) == chess::W_KING);
    assert(board.pieceOn(chess::E8) == chess::B_KING);
    assert(board.pieceOn(chess::A1) == chess::W_ROOK);
    assert(board.pieceOn(chess::H1) == chess::W_ROOK);
    assert(board.pieceOn(chess::A8) == chess::B_ROOK);
    assert(board.pieceOn(chess::H8) == chess::B_ROOK);

    // FEN roundtrip
    std::string fen = board.fen();
    chess::Board board2(fen);
    assert(board.sideToMove() == board2.sideToMove());
    assert(board.castlingRights() == board2.castlingRights());
    assert(board.enPassant() == board2.enPassant());

    // Make a move: e2e4
    chess::UndoInfo undo;
    chess::Move e4(chess::E2, chess::E4);
    bool ok = board.makeMove(e4, undo);
    assert(ok);
    assert(board.sideToMove() == chess::BLACK);
    assert(board.pieceOn(chess::E4) == chess::W_PAWN);
    assert(board.pieceOn(chess::E2) == chess::NO_PIECE);
    assert(board.enPassant() == chess::E3);
    assert(!board.isInCheck());

    // Unmake
    board.unmakeMove(e4, undo);
    assert(board.sideToMove() == chess::WHITE);
    assert(board.pieceOn(chess::E2) == chess::W_PAWN);
    assert(board.pieceOn(chess::E4) == chess::NO_PIECE);

    std::cout << "All board tests passed." << std::endl;
    return 0;
}
