#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/types.h"
#include <iostream>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

void test_startPosition() {
    chess::Board b;
    CHECK(b.sideToMove() == chess::WHITE);
    CHECK(b.castlingRights() == chess::ALL);
    CHECK(b.enPassant() == chess::SQ_NONE);
    CHECK(b.pieceOn(chess::E1) == chess::W_KING);
    CHECK(b.pieceOn(chess::E8) == chess::B_KING);
    CHECK(b.pieceOn(chess::A2) == chess::W_PAWN);
    CHECK(b.pieceOn(chess::D7) == chess::B_PAWN);
    CHECK(b.pieces(chess::WHITE, chess::PAWN) == 0x000000000000FF00ULL);
    CHECK(b.pieces(chess::BLACK, chess::PAWN) == 0x00FF000000000000ULL);
    CHECK(!b.isInCheck());
}

void test_fenParsing() {
    std::string fen = "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";
    chess::Board b(fen);
    CHECK(b.pieceOn(chess::B5) == chess::W_BISHOP);
    CHECK(b.pieceOn(chess::C6) == chess::B_KNIGHT);
    CHECK(b.pieceOn(chess::F3) == chess::W_KNIGHT);
    CHECK(b.sideToMove() == chess::WHITE);
    CHECK(b.castlingRights() == chess::ALL);
}

void test_fenRoundtrip() {
    std::string fen1 = "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";
    chess::Board b(fen1);
    std::string fen2 = b.fen();
    CHECK(fen1 == fen2);
}

void test_makeMove_basic() {
    chess::Board b;
    chess::UndoInfo undo;
    chess::Move e4(chess::E2, chess::E4);
    CHECK(b.makeMove(e4, undo));
    CHECK(b.sideToMove() == chess::BLACK);
    CHECK(b.pieceOn(chess::E4) == chess::W_PAWN);
    CHECK(b.pieceOn(chess::E2) == chess::NO_PIECE);
}

void test_makeMove_doublePush() {
    chess::Board b;
    chess::UndoInfo undo;
    chess::Move e4(chess::E2, chess::E4);
    b.makeMove(e4, undo);
    CHECK(b.enPassant() == chess::E3);
}

void test_makeMove_unmake() {
    chess::Board b;
    chess::UndoInfo undo;
    chess::Move e4(chess::E2, chess::E4);
    b.makeMove(e4, undo);
    b.unmakeMove(e4, undo);
    CHECK(b.sideToMove() == chess::WHITE);
    CHECK(b.pieceOn(chess::E2) == chess::W_PAWN);
    CHECK(b.pieceOn(chess::E4) == chess::NO_PIECE);
    CHECK(b.enPassant() == chess::SQ_NONE);
}

void test_castling() {
    std::string fen = "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/2N2N2/PPPPBPPP/R1BQK2R w KQkq - 4 5";
    chess::Board b(fen);
    chess::UndoInfo undo;
    chess::Move oo(chess::E1, chess::G1);
    CHECK(b.makeMove(oo, undo));
    CHECK(b.pieceOn(chess::G1) == chess::W_KING);
    CHECK(b.pieceOn(chess::F1) == chess::W_ROOK);
    CHECK(b.pieceOn(chess::H1) == chess::NO_PIECE);
    CHECK(b.pieceOn(chess::E1) == chess::NO_PIECE);
    CHECK((b.castlingRights() & chess::WK) == 0);
}

void test_illegalSelfCheck() {
    std::string fen = "4k3/8/8/8/8/8/8/3K4 w - - 0 1";
    chess::Board b(fen);
    chess::UndoInfo undo;
    chess::Move kd2(chess::D1, chess::D2);
    CHECK(b.makeMove(kd2, undo));
}

void test_kingInCheck() {
    std::string fen = "4k3/8/8/8/8/8/3q4/3K4 w - - 0 1";
    chess::Board b(fen);
    CHECK(b.isInCheck());
}

void test_enPassantCapture() {
    std::string fen = "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3";
    chess::Board b(fen);
    CHECK(b.enPassant() == chess::D6);
    chess::UndoInfo undo;
    chess::Move ep(chess::E5, chess::D6);
    CHECK(b.makeMove(ep, undo));
    CHECK(b.pieceOn(chess::D5) == chess::NO_PIECE);
    CHECK(b.pieceOn(chess::D6) == chess::W_PAWN);
    b.unmakeMove(ep, undo);
    CHECK(b.pieceOn(chess::D5) == chess::B_PAWN);
    CHECK(b.pieceOn(chess::D6) == chess::NO_PIECE);
    CHECK(b.pieceOn(chess::E5) == chess::W_PAWN);
    CHECK(b.enPassant() == chess::D6);
}

void test_promotion() {
    std::string fen = "8/P7/8/8/8/8/8/k6K w - - 0 1";
    chess::Board b(fen);
    chess::UndoInfo undo;
    chess::Move promo(chess::A7, chess::A8, chess::QUEEN);
    CHECK(b.makeMove(promo, undo));
    CHECK(b.pieceOn(chess::A8) == chess::W_QUEEN);
    CHECK(b.pieceOn(chess::A7) == chess::NO_PIECE);
    b.unmakeMove(promo, undo);
    CHECK(b.pieceOn(chess::A7) == chess::W_PAWN);
    CHECK(b.pieceOn(chess::A8) == chess::NO_PIECE);
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running board tests:" << std::endl;
    RUN_TEST(startPosition);
    RUN_TEST(fenParsing);
    RUN_TEST(fenRoundtrip);
    RUN_TEST(makeMove_basic);
    RUN_TEST(makeMove_doublePush);
    RUN_TEST(makeMove_unmake);
    RUN_TEST(castling);
    RUN_TEST(illegalSelfCheck);
    RUN_TEST(kingInCheck);
    RUN_TEST(enPassantCapture);
    RUN_TEST(promotion);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll board tests passed." << std::endl;
    return 0;
}
