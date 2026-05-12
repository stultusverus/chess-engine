#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/movegen.h"
#include "engine/types.h"
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

static const chess::Move* findMove(const chess::MoveList& moves, chess::Square from,
                                   chess::Square to,
                                   chess::PieceType promotion = chess::PIECE_TYPE_NB) {
    for (const chess::Move& m : moves) {
        if (m.from == from && m.to == to && m.promotion == promotion)
            return &m;
    }
    return nullptr;
}

static bool isNoisy(const chess::Move& move) {
    return move.type == chess::CAPTURE ||
           move.type == chess::EN_PASSANT ||
           move.type == chess::PROMOTION ||
           move.type == chess::PROMOTION_CAPTURE;
}

static bool sameMove(const chess::Move& a, const chess::Move& b) {
    return a.from == b.from &&
           a.to == b.to &&
           a.promotion == b.promotion &&
           a.type == b.type;
}

static bool containsExactMove(const chess::MoveList& moves, const chess::Move& target) {
    for (const chess::Move& m : moves) {
        if (sameMove(m, target))
            return true;
    }
    return false;
}

static void checkNoisyMatchesFilteredLegalMoves(const std::string& fen) {
    chess::MoveGenerator gen;
    chess::Board fullBoard(fen);
    chess::Board noisyBoard(fen);
    chess::MoveList fullMoves;
    chess::MoveList noisyMoves;
    chess::MoveList expected;

    gen.generateMoves(fullBoard, fullMoves);
    gen.generateLegalNoisyMoves(noisyBoard, noisyMoves);

    for (const chess::Move& m : fullMoves) {
        if (isNoisy(m))
            expected.add(m);
    }

    CHECK(noisyMoves.size() == expected.size());
    for (const chess::Move& m : noisyMoves) {
        CHECK(isNoisy(m));
        CHECK(containsExactMove(expected, m));
    }
    for (const chess::Move& m : expected) {
        CHECK(containsExactMove(noisyMoves, m));
    }
}

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
    std::string fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    chess::Board b(fen);
    CHECK(gen.perft(b, 1) == 48);
    CHECK(gen.perft(b, 2) == 2039);
    CHECK(gen.perft(b, 3) == 97862);
    std::cout << "     perft(4)=" << gen.perft(b, 4) << " (expect 4085603)" << std::endl;
}

// Position 3
void test_pos3_perft() {
    chess::MoveGenerator gen;
    std::string fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
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
    chess::Board b("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    chess::MoveList moves;
    gen.generateMoves(b, moves);

    for (const chess::Move& m : moves) {
        CHECK(!(m.from == chess::E1 && m.to == chess::G1));
    }
}

void test_generatedMoveTypesAreClassified() {
    chess::MoveGenerator gen;
    chess::MoveList moves;

    // Expected: generated captures are marked as captures for search ordering and qsearch.
    chess::Board capture("4k3/8/8/3q4/3Q4/8/8/4K3 w - - 0 1");
    gen.generateMoves(capture, moves);
    const chess::Move* queenCapture = findMove(moves, chess::D4, chess::D5);
    CHECK(queenCapture != nullptr);
    if (queenCapture) CHECK(queenCapture->type == chess::CAPTURE);

    // Expected: generated promotions carry PROMOTION metadata.
    moves.clear();
    chess::Board promotion("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    gen.generateMoves(promotion, moves);
    const chess::Move* queenPromotion = findMove(moves, chess::A7, chess::A8, chess::QUEEN);
    CHECK(queenPromotion != nullptr);
    if (queenPromotion) CHECK(queenPromotion->type == chess::PROMOTION);

    // Expected: generated en-passant captures carry EN_PASSANT metadata.
    moves.clear();
    chess::Board ep("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    gen.generateMoves(ep, moves);
    const chess::Move* epCapture = findMove(moves, chess::E5, chess::D6);
    CHECK(epCapture != nullptr);
    if (epCapture) CHECK(epCapture->type == chess::EN_PASSANT);

    // Expected: generated castling moves carry CASTLING metadata.
    moves.clear();
    chess::Board castle("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    gen.generateMoves(castle, moves);
    const chess::Move* whiteCastle = findMove(moves, chess::E1, chess::G1);
    CHECK(whiteCastle != nullptr);
    if (whiteCastle) CHECK(whiteCastle->type == chess::CASTLING);
}

void test_generateMovesClearsOutputList() {
    chess::MoveGenerator gen;
    chess::MoveList moves;

    chess::Board start;
    gen.generateMoves(start, moves);
    CHECK(moves.size() == 20);

    // Expected: the second call replaces the old contents instead of appending to them.
    chess::Board checkmate("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    gen.generateMoves(checkmate, moves);
    CHECK(moves.size() == 0);
}

void test_generateLegalNoisyMovesMatchesFilteredLegalMoves() {
    checkNoisyMatchesFilteredLegalMoves("4k3/8/8/3q4/3Q4/8/8/4K3 w - - 0 1");
    checkNoisyMatchesFilteredLegalMoves("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    checkNoisyMatchesFilteredLegalMoves("4k2r/6P1/8/8/8/8/8/4K3 w - - 0 1");
    checkNoisyMatchesFilteredLegalMoves("4r1k1/8/8/8/8/8/3nR3/4K3 w - - 0 1");
    checkNoisyMatchesFilteredLegalMoves("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
}

void test_generateLegalNoisyMovesFiltersIllegalPinnedCaptures() {
    chess::MoveGenerator gen;
    chess::MoveList moves;
    chess::Board pinned("4r1k1/8/8/8/8/8/3nR3/4K3 w - - 0 1");

    gen.generateLegalNoisyMoves(pinned, moves);

    CHECK(findMove(moves, chess::E2, chess::D2) == nullptr);
    CHECK(findMove(moves, chess::E2, chess::E8) != nullptr);
}

void test_checkEvasionAllowsDoublePawnBlock() {
    chess::MoveGenerator gen;
    chess::MoveList moves;
    chess::Board b("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");

    CHECK(b.isInCheck());
    gen.generateMoves(b, moves);

    CHECK(findMove(moves, chess::D2, chess::D4) != nullptr);
}

void test_enPassantCannotExposeHorizontalCheck() {
    chess::MoveGenerator gen;
    chess::MoveList moves;
    chess::Board b("4k3/8/8/r2pP2K/8/8/8/8 w - d6 0 1");

    gen.generateMoves(b, moves);

    CHECK(findMove(moves, chess::E5, chess::D6) == nullptr);
    CHECK(!b.isMoveLegal(chess::Move(chess::E5, chess::D6)));
}

void test_enPassantCanCaptureCheckingPawn() {
    chess::MoveGenerator gen;
    chess::MoveList moves;
    chess::Board b("4k3/8/8/3pP3/4K3/8/8/8 w - d6 0 1");

    CHECK(b.isInCheck());
    gen.generateMoves(b, moves);

    const chess::Move* epCapture = findMove(moves, chess::E5, chess::D6);
    CHECK(epCapture != nullptr);
    if (epCapture)
        CHECK(epCapture->type == chess::EN_PASSANT);
}

void test_generateLegalNoisyMovesClearsOutputList() {
    chess::MoveGenerator gen;
    chess::MoveList moves;

    chess::Board capture("4k3/8/8/3q4/3Q4/8/8/4K3 w - - 0 1");
    gen.generateLegalNoisyMoves(capture, moves);
    CHECK(moves.size() > 0);

    chess::Board quiet("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    gen.generateLegalNoisyMoves(quiet, moves);
    CHECK(moves.size() == 0);
}

void test_hasLegalMoveDetectsStalemate() {
    chess::MoveGenerator gen;
    chess::Board start;
    chess::Board stalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

    CHECK(gen.hasLegalMove(start));
    CHECK(!stalemate.isInCheck());
    CHECK(!gen.hasLegalMove(stalemate));
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
    RUN_TEST(generatedMoveTypesAreClassified);
    RUN_TEST(generateMovesClearsOutputList);
    RUN_TEST(generateLegalNoisyMovesMatchesFilteredLegalMoves);
    RUN_TEST(generateLegalNoisyMovesFiltersIllegalPinnedCaptures);
    RUN_TEST(checkEvasionAllowsDoublePawnBlock);
    RUN_TEST(enPassantCannotExposeHorizontalCheck);
    RUN_TEST(enPassantCanCaptureCheckingPawn);
    RUN_TEST(generateLegalNoisyMovesClearsOutputList);
    RUN_TEST(hasLegalMoveDetectsStalemate);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll perft tests passed." << std::endl;
    return 0;
}
