#include "engine/attacks.h"
#include "engine/board.h"
#include "engine/eval.h"
#include "engine/see.h"
#include "engine/types.h"

#include <iostream>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

void test_hangingQueenCapture() {
    chess::Board board("q3k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::A1, chess::A8, chess::PIECE_TYPE_NB, chess::CAPTURE));
    CHECK(see == chess::Eval::QUEEN_VALUE);
}

void test_losingQueenCapture() {
    chess::Board board("3rk3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::D4, chess::D5, chess::PIECE_TYPE_NB, chess::CAPTURE));
    CHECK(see < 0);
    CHECK(see <= -700);
}

void test_equalRookTradeWithKingRecapture() {
    chess::Board board("3rk3/8/8/8/8/8/8/3RK3 w - - 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::D1, chess::D8, chess::PIECE_TYPE_NB, chess::CAPTURE));
    CHECK(see == 0);
}

void test_pinnedRecapturerIgnored() {
    chess::Board board("4k3/4b3/3p4/5N2/8/8/8/4R1K1 w - - 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::F5, chess::D6, chess::PIECE_TYPE_NB, chess::CAPTURE));
    CHECK(see == chess::Eval::PAWN_VALUE);
}

void test_promotionCaptureIncludesPromotionGain() {
    chess::Board board("r3k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::B7, chess::A8, chess::QUEEN, chess::PROMOTION_CAPTURE));
    CHECK(see >= chess::Eval::ROOK_VALUE);
    CHECK(see == chess::Eval::ROOK_VALUE + chess::Eval::QUEEN_VALUE - chess::Eval::PAWN_VALUE);
}

void test_enPassantCapture() {
    chess::Board board("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    int see = chess::staticExchangeEval(board, chess::Move(chess::E5, chess::D6, chess::PIECE_TYPE_NB, chess::EN_PASSANT));
    CHECK(see == chess::Eval::PAWN_VALUE);
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running SEE tests:" << std::endl;
    RUN_TEST(hangingQueenCapture);
    RUN_TEST(losingQueenCapture);
    RUN_TEST(equalRookTradeWithKingRecapture);
    RUN_TEST(pinnedRecapturerIgnored);
    RUN_TEST(promotionCaptureIncludesPromotionGain);
    RUN_TEST(enPassantCapture);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll SEE tests passed." << std::endl;
    return 0;
}
