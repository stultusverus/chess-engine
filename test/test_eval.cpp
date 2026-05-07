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

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll eval tests passed." << std::endl;
    return 0;
}
