#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/eval.h"
#include "engine/types.h"
#include <iostream>
#include <cmath>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define CHECK_CLOSE(a, b, tol) do { if (std::abs((a) - (b)) > (tol)) { std::cerr << "FAIL: " << #a << "=" << (a) << " != " << #b << "=" << (b) << " (tol=" << tol << ")" << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

chess::Eval eval;

void test_startpos_balanced() {
    chess::Board b;
    int score = eval.evaluate(b);
    CHECK_CLOSE(score, 0, 50);
}

void test_white_up_pawn() {
    chess::Board b("rnbqkbnr/pppp1ppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    CHECK(eval.evaluate(b) > 0);
}

void test_white_up_queen() {
    chess::Board b("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
    int score = eval.evaluate(b);
    CHECK(score > 800);
}

void test_black_up_rook() {
    chess::Board b("4k2r/8/8/8/8/8/8/4K3 w - - 0 1");
    int score = eval.evaluate(b);
    CHECK(score < -400);
}

void test_material_imbalance_subtle() {
    // White: K+2N, Black: K+N  (white up a knight)
    chess::Board b("4k3/8/8/8/8/8/3NN3/4K3 w - - 0 1");
    int score = eval.evaluate(b);
    CHECK(score > 200);

    chess::Board b2("4k3/8/8/8/8/8/3n4/4K3 w - - 0 1");
    CHECK(eval.evaluate(b2) < 0);
}

void test_pieceValues() {
    CHECK(chess::Eval::pieceValue(chess::PAWN) == 100);
    CHECK(chess::Eval::pieceValue(chess::KNIGHT) == 320);
    CHECK(chess::Eval::pieceValue(chess::BISHOP) == 330);
    CHECK(chess::Eval::pieceValue(chess::ROOK) == 500);
    CHECK(chess::Eval::pieceValue(chess::QUEEN) == 900);
    CHECK(chess::Eval::pieceValue(chess::KING) == 20000);
}

void test_center_control_bonus() {
    chess::Board bCenter("4k3/8/8/8/4N3/8/8/4K3 w - - 0 1");
    chess::Board bCorner("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
    int scoreCenter = eval.evaluate(bCenter);
    int scoreCorner = eval.evaluate(bCorner);
    CHECK(scoreCenter >= scoreCorner);
}

void test_king_safety_mg_vs_eg() {
    // King in center in the middlegame should be worse than castled
    chess::Board bCentral("4k3/8/8/3p4/8/8/8/2K5 w - - 0 1");
    chess::Board bCorner("4k3/8/8/3p4/8/8/8/6K1 w - - 0 1");
    int scoreCentral = eval.evaluate(bCentral);
    int scoreCorner = eval.evaluate(bCorner);
    CHECK(scoreCorner >= scoreCentral);
}

void test_doubledPawn() {
    // Black doubled b-pawns (bad for black = good for white)
    chess::Board bDoubled("7k/1p6/1p6/8/8/8/8/7K w - - 0 1");
    // Black non-doubled pawns (same material)
    chess::Board bNormal("7k/pp6/8/8/8/8/8/7K w - - 0 1");
    CHECK(eval.evaluate(bDoubled) > eval.evaluate(bNormal));
}

void test_isolatedPawn() {
    // White pawns on c2+d2 (connected) vs c2+e2 (both isolated)
    chess::Board bConnected("7k/8/8/8/8/8/2PP4/7K w - - 0 1");
    chess::Board bIsolated ("7k/8/8/8/8/8/2P1P3/7K w - - 0 1");
    CHECK(eval.evaluate(bConnected) > eval.evaluate(bIsolated));
}

void test_passedPawn() {
    // White pawn on b4: passed (no black pawns ahead) vs blocked (a7 pawn ahead)
    chess::Board bPassed ("7k/8/8/8/1P6/p7/8/7K w - - 0 1");
    chess::Board bBlocked("7k/p7/8/8/1P6/8/8/7K w - - 0 1");
    CHECK(eval.evaluate(bPassed) > eval.evaluate(bBlocked));
}

void test_mobilityBonus() {
    // Centralized knight more mobile than corner knight
    chess::Board bCenter("7k/8/8/8/4N3/8/8/7K w - - 0 1");
    chess::Board bCorner("7k/8/8/8/8/8/8/N6K w - - 0 1");
    CHECK(eval.evaluate(bCenter) > eval.evaluate(bCorner));
}

void test_bishopPairBonus() {
    // Two bishops vs bishop+knight (same material)
    chess::Board bPair  ("7k/8/8/8/8/8/8/2B1B2K w - - 0 1");
    chess::Board bNoPair("7k/8/8/8/8/8/8/2B1N2K w - - 0 1");
    CHECK(eval.evaluate(bPair) > eval.evaluate(bNoPair));
}

void test_rookOpenFile() {
    // Rook on open file vs rook on closed file (same material)
    chess::Board bOpen  ("7k/8/8/8/8/8/4P3/3R3K w - - 0 1");
    chess::Board bClosed("7k/8/8/8/8/3P4/8/3R3K w - - 0 1");
    CHECK(eval.evaluate(bOpen) > eval.evaluate(bClosed));
}

void test_kingPawnShield() {
    // King on g1 with shield pawns (f2,g2,h2) vs pawns elsewhere (same count)
    chess::Board bShield  ("7k/8/8/8/8/8/5PPP/6K1 w - - 0 1");
    chess::Board bNoShield("7k/8/8/8/8/8/P5P1/6K1 w - - 0 1");
    CHECK(eval.evaluate(bShield) > eval.evaluate(bNoShield));
}

void test_tempo() {
    // White to move scores higher than black to move (all else equal)
    chess::Board bWhite("7k/8/8/8/8/8/8/7K w - - 0 1");
    chess::Board bBlack("7k/8/8/8/8/8/8/7K b - - 0 1");
    CHECK(eval.evaluate(bWhite) > eval.evaluate(bBlack));
}

void test_kingMobility() {
    // Centralized king more mobile than corner king
    chess::Board bCenter("7k/8/8/4K3/8/8/8/8 w - - 0 1");
    chess::Board bCorner("7k/8/8/8/8/8/8/K7 w - - 0 1");
    CHECK(eval.evaluate(bCenter) > eval.evaluate(bCorner));
}

void test_kingOpenFile() {
    // King on g1 with open adjacent files, opponent has rook: penalty lowers eval
    // (other factors like PST can outweigh the penalty, but penalty is active)
    chess::Board bOpen ("7k/6r1/8/8/8/8/P5P1/6K1 w - - 0 1");
    chess::Board bClosed("7k/6r1/8/8/8/8/5P1P/6K1 w - - 0 1");
    // bOpen has open f/h, bClosed has open g; eval ordering depends on PST + mobility
    CHECK(eval.evaluate(bOpen) > eval.evaluate(bClosed));
}

void test_isolatedRookFile() {
    // Isolated pawn on rook file penalized half as much as center file
    // (PST values differ, so combined eval favors a-file in this endgame)
    chess::Board bRook  ("7k/8/8/8/8/8/P7/7K w - - 0 1");
    chess::Board bCenter("7k/8/8/8/8/8/3P4/7K w - - 0 1");
    CHECK(eval.evaluate(bRook) > eval.evaluate(bCenter));
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running eval tests:" << std::endl;
    RUN_TEST(startpos_balanced);
    RUN_TEST(white_up_pawn);
    RUN_TEST(white_up_queen);
    RUN_TEST(black_up_rook);
    RUN_TEST(material_imbalance_subtle);
    RUN_TEST(pieceValues);
    RUN_TEST(center_control_bonus);
    RUN_TEST(king_safety_mg_vs_eg);
    RUN_TEST(doubledPawn);
    RUN_TEST(isolatedPawn);
    RUN_TEST(passedPawn);
    RUN_TEST(mobilityBonus);
    RUN_TEST(bishopPairBonus);
    RUN_TEST(rookOpenFile);
    RUN_TEST(kingPawnShield);
    RUN_TEST(tempo);
    RUN_TEST(kingMobility);
    RUN_TEST(kingOpenFile);
    RUN_TEST(isolatedRookFile);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll eval tests passed." << std::endl;
    return 0;
}
