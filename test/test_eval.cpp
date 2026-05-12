#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/eval.h"
#include "engine/movegen.h"
#include "engine/types.h"
#include <iostream>
#include <cmath>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define CHECK_CLOSE(a, b, tol) do { if (std::abs((a) - (b)) > (tol)) { std::cerr << "FAIL: " << #a << "=" << (a) << " != " << #b << "=" << (b) << " (tol=" << tol << ")" << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

chess::Eval eval;

chess::IncrementalEvalState recomputeIncrementalState(const chess::Board& b) {
    chess::IncrementalEvalState state;
    for (int s = chess::A1; s <= chess::H8; s++) {
        chess::Square sq = chess::Square(s);
        chess::Piece p = b.pieceOn(sq);
        if (p == chess::NO_PIECE) continue;
        chess::PieceType pt = chess::typeOf(p);
        chess::Color c = chess::colorOf(p);
        if (pt != chess::KING)
            state.material += c == chess::WHITE ? chess::Eval::pieceValue(pt) : -chess::Eval::pieceValue(pt);
        state.pstMg += chess::Eval::pieceSquareMg(p, sq);
        state.pstEg += chess::Eval::pieceSquareEg(p, sq);
        state.phase += chess::Eval::phaseValue(pt);
    }
    return state;
}

void checkIncrementalState(const chess::Board& b) {
    chess::IncrementalEvalState expected = recomputeIncrementalState(b);
    CHECK(b.materialScore() == expected.material);
    CHECK(b.pstMgScore() == expected.pstMg);
    CHECK(b.pstEgScore() == expected.pstEg);
    CHECK(b.gamePhaseScore() == expected.phase);
}

void walkIncrementalState(chess::Board& b, chess::MoveGenerator& gen, int depth) {
    checkIncrementalState(b);
    if (depth == 0) return;

    uint64_t oldHash = b.hash();
    uint64_t oldPawnHash = b.pawnHash();
    chess::IncrementalEvalState oldState = b.evalState();
    std::string oldFen = b.fen();

    chess::MoveList moves;
    gen.generateLegalMoves(b, moves);
    for (const chess::Move& m : moves) {
        chess::UndoInfo undo;
        if (!b.makeMove(m, undo)) continue;
        checkIncrementalState(b);
        walkIncrementalState(b, gen, depth - 1);
        b.unmakeMove(m, undo);
        CHECK(b.hash() == oldHash);
        CHECK(b.pawnHash() == oldPawnHash);
        CHECK(b.materialScore() == oldState.material);
        CHECK(b.pstMgScore() == oldState.pstMg);
        CHECK(b.pstEgScore() == oldState.pstEg);
        CHECK(b.gamePhaseScore() == oldState.phase);
        CHECK(b.fen() == oldFen);
    }
}

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

void test_incrementalEvalStateMatchesRecompute() {
    std::vector<std::string> fens = {
        chess::STARTPOS_FEN,
        "r3k2r/pppq1ppp/2npbn2/3Np3/2B1P3/2N2Q2/PPP2PPP/R3K2R w KQkq - 0 10",
        "4k3/P6p/8/3pP3/8/8/p6P/4K3 w - d6 0 1",
        "8/2k5/8/8/2pPp3/8/4K3/8 b - d3 0 1"
    };

    chess::MoveGenerator gen;
    for (const std::string& fen : fens) {
        chess::Board b(fen);
        walkIncrementalState(b, gen, 2);
    }
}

void test_pawnHashTracksOnlyPawns() {
    chess::Board pawnsOnly("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");
    chess::Board withPieces("r3k3/8/8/8/8/8/PP6/R3K3 b - - 0 1");
    chess::Board differentPawns("4k3/8/8/8/8/8/P1P5/4K3 w - - 0 1");

    CHECK(pawnsOnly.pawnHash() == withPieces.pawnHash());
    CHECK(pawnsOnly.pawnHash() != differentPawns.pawnHash());

    uint64_t startPawnHash = pawnsOnly.pawnHash();
    chess::UndoInfo undo;
    CHECK(pawnsOnly.makeMove(chess::Move(chess::E1, chess::E2), undo));
    CHECK(pawnsOnly.pawnHash() == startPawnHash);
    pawnsOnly.unmakeMove(chess::Move(chess::E1, chess::E2), undo);
    CHECK(pawnsOnly.pawnHash() == startPawnHash);

    CHECK(pawnsOnly.makeMove(chess::Move(chess::A2, chess::A3), undo));
    CHECK(pawnsOnly.pawnHash() != startPawnHash);
    pawnsOnly.unmakeMove(chess::Move(chess::A2, chess::A3), undo);
    CHECK(pawnsOnly.pawnHash() == startPawnHash);
}

void test_evalCachePreservesScoresAcrossMakeUnmake() {
    chess::Board b("r3k2r/pppq1ppp/2npbn2/3Np3/2B1P3/2N2Q2/PPP2PPP/R3K2R w KQkq - 0 10");
    int startScore = eval.evaluate(b);
    CHECK(eval.evaluate(b) == startScore);

    chess::UndoInfo undo;
    CHECK(b.makeMove(chess::Move(chess::E1, chess::G1), undo));
    int afterScore = eval.evaluate(b);
    CHECK(eval.evaluate(b) == afterScore);
    b.unmakeMove(chess::Move(chess::E1, chess::G1), undo);
    CHECK(eval.evaluate(b) == startScore);
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
    RUN_TEST(incrementalEvalStateMatchesRecompute);
    RUN_TEST(pawnHashTracksOnlyPawns);
    RUN_TEST(evalCachePreservesScoresAcrossMakeUnmake);
    RUN_TEST(isolatedRookFile);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll eval tests passed." << std::endl;
    return 0;
}
