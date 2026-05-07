#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/book.h"
#include "engine/types.h"
#include <iostream>
#include <cmath>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

void test_decodeNormalMove() {
    // Polyglot packed move for e2e4: from=e2(12), to=e4(28)
    // pack = (12 << 6) | 28 = 796
    uint16_t packed = (12 << 6) | 28;
    auto m = chess::Book::decodePolyglotMove(packed);
    CHECK(m.from == chess::E2);
    CHECK(m.to == chess::E4);
    CHECK(m.promotion == chess::PIECE_TYPE_NB);
}

void test_decodePromotionMove() {
    // Promotion a7a8q: from=a7(48), to=a8(56), promo=queen(4)
    // pack = (4 << 12) | (48 << 6) | 56
    uint16_t packed = (4 << 12) | (48 << 6) | 56;
    auto m = chess::Book::decodePolyglotMove(packed);
    CHECK(m.from == chess::A7);
    CHECK(m.to == chess::A8);
    CHECK(m.promotion == chess::QUEEN);
}

void test_decodeRemapped() {
    // Known book entry for startpos: d2d4 = from=d2(11), to=d4(27)
    // pack = (11 << 6) | 27 = 731
    uint16_t packed = (11u << 6) | 27;
    auto m = chess::Book::decodePolyglotMove(packed);
    CHECK(m.from == chess::D2);
    CHECK(m.to == chess::D4);
}

void test_hashStartpos() {
    // python-chess reference: 0x463B96181691FC9C
    chess::Board b;
    uint64_t hash = chess::Book::polyglotHash(b);
    CHECK(hash == 0x463B96181691FC9CULL);
}

void test_hashAfterE4() {
    // python-chess reference: 0x823C9B50FD114196
    chess::Board b;
    chess::UndoInfo undo;
    b.makeMove(chess::Move(chess::E2, chess::E4), undo);
    uint64_t hash = chess::Book::polyglotHash(b);
    CHECK(hash == 0x823C9B50FD114196ULL);
}

void test_hashAfterE4E5() {
    // python-chess reference: 0x0844931A6EF4B9A0
    chess::Board b;
    chess::UndoInfo undo;
    b.makeMove(chess::Move(chess::E2, chess::E4), undo);
    b.makeMove(chess::Move(chess::E7, chess::E5), undo);
    uint64_t hash = chess::Book::polyglotHash(b);
    CHECK(hash == 0x0844931A6EF4B9A0ULL);
}

void test_hashDifferentSides() {
    // Same pieces, different side to move should give different hashes
    chess::Board bWhite("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    chess::Board bBlack("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    CHECK(chess::Book::polyglotHash(bWhite) != chess::Book::polyglotHash(bBlack));
}

void test_loadValidBook() {
    chess::Book book;
    bool loaded = book.load("../books/gm2001.bin");
    CHECK(loaded);
    CHECK(book.isLoaded());
}

void test_loadInvalidBook() {
    chess::Book book;
    bool loaded = book.load("/nonexistent/book.bin");
    CHECK(!loaded);
    CHECK(!book.isLoaded());
}

void test_probeStartpos() {
    chess::Book book;
    book.load("../books/gm2001.bin");
    chess::Board b;
    auto m = book.probe(b);
    CHECK(m.has_value());
    // Should be a well-known opening move
    chess::Square sq = m->from;
    (void)sq;
}

void test_probeEmptyBook() {
    chess::Book book;
    chess::Board b;
    auto m = book.probe(b);
    CHECK(!m.has_value());
}

void test_maxPlyCapped() {
    chess::Book book;
    book.load("../books/gm2001.bin");
    book.setMaxPly(1); // 1 ply = 2 half-moves

    // Startpos: 0 half-moves — should probe
    {
        chess::Board b;
        auto m = book.probe(b);
        CHECK(m.has_value());
    }

    // After e2e4 e7e5: 2 half-moves — at the cap, should not probe
    {
        chess::Board b;
        chess::UndoInfo undo;
        b.makeMove(chess::Move(chess::E2, chess::E4), undo);
        b.makeMove(chess::Move(chess::E7, chess::E5), undo);
        auto m = book.probe(b);
        CHECK(!m.has_value());
    }
}

void test_weightedRandomProducesValidMoves() {
    chess::Book book;
    book.load("../books/gm2001.bin");
    chess::Board b;

    // Probe 20 times and verify all results are valid moves for startpos
    for (int i = 0; i < 20; i++) {
        auto m = book.probe(b);
        CHECK(m.has_value());
        // Verify the move is legal on the board
        CHECK(b.isMoveLegal(*m));
    }
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Book tests:" << std::endl;

    RUN_TEST(decodeNormalMove);
    RUN_TEST(decodePromotionMove);
    RUN_TEST(decodeRemapped);
    RUN_TEST(hashStartpos);
    RUN_TEST(hashAfterE4);
    RUN_TEST(hashAfterE4E5);
    RUN_TEST(hashDifferentSides);
    RUN_TEST(loadValidBook);
    RUN_TEST(loadInvalidBook);
    RUN_TEST(probeStartpos);
    RUN_TEST(probeEmptyBook);
    RUN_TEST(maxPlyCapped);
    RUN_TEST(weightedRandomProducesValidMoves);

    std::cout << (failures ? "SOME TESTS FAILED" : "All book tests passed") << std::endl;
    return failures ? 1 : 0;
}
