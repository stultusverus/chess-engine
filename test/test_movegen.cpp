#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/movegen.h"
#include "engine/types.h"
#include <iostream>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

void test_startpos_perft() {
    chess::MoveGenerator gen;
    chess::Board b;
    CHECK(gen.perft(b, 1) == 20);
    CHECK(gen.perft(b, 2) == 400);
    CHECK(gen.perft(b, 3) == 8902);
    CHECK(gen.perft(b, 4) == 197281);
    std::cout << "     perft(5)=" << gen.perft(b, 5) << " (expect 4865609)" << std::endl;
}

// Kiwipete position (perft by Steven Edwards)
void test_kiwipete_perft() {
    chess::MoveGenerator gen;
    std::string fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 0";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 48);
    CHECK(gen.perft(b, 2) == 2039);
    CHECK(gen.perft(b, 3) == 97862);
    std::cout << "     perft(4)=" << gen.perft(b, 4) << " (expect 4085603)" << std::endl;
}

// Position 3
void test_pos3_perft() {
    chess::MoveGenerator gen;
    std::string fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 14);
    CHECK(gen.perft(b, 2) == 191);
    CHECK(gen.perft(b, 3) == 2812);
    CHECK(gen.perft(b, 4) == 43238);
    CHECK(gen.perft(b, 5) == 674624);
    std::cout << "     perft(6)=" << gen.perft(b, 6) << " (expect 11030083)" << std::endl;
}

// Position 4
void test_pos4_perft() {
    chess::MoveGenerator gen;
    std::string fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 6);
    CHECK(gen.perft(b, 2) == 264);
    CHECK(gen.perft(b, 3) == 9467);
    CHECK(gen.perft(b, 4) == 422333);
    std::cout << "     perft(5)=" << gen.perft(b, 5) << " (expect 15833292)" << std::endl;
}

// Position 5
void test_pos5_perft() {
    chess::MoveGenerator gen;
    std::string fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 44);
    CHECK(gen.perft(b, 2) == 1486);
    CHECK(gen.perft(b, 3) == 62379);
    std::cout << "     perft(4)=" << gen.perft(b, 4) << " (expect 2103487)" << std::endl;
}

// Position 6
void test_pos6_perft() {
    chess::MoveGenerator gen;
    std::string fen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 46);
    CHECK(gen.perft(b, 2) == 2079);
    CHECK(gen.perft(b, 3) == 89890);
    std::cout << "     perft(4)=" << gen.perft(b, 4) << " (expect 3894594)" << std::endl;
}

void test_noCastlingWithoutRook() {
    chess::MoveGenerator gen;
    chess::Board b("4k3/8/8/8/8/8/8/4K3 w K - 0 1");
    chess::MoveList moves;
    gen.generateMoves(b, moves);

    for (const chess::Move& m : moves) {
        CHECK(!(m.from == chess::E1 && m.to == chess::G1));
    }
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running movegen tests:" << std::endl;
    RUN_TEST(startpos_perft);
    RUN_TEST(kiwipete_perft);
    RUN_TEST(pos3_perft);
    RUN_TEST(pos4_perft);
    RUN_TEST(pos5_perft);
    RUN_TEST(pos6_perft);
    RUN_TEST(noCastlingWithoutRook);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll perft tests passed." << std::endl;
    return 0;
}
