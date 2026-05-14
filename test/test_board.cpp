#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/movegen.h"
#include "engine/types.h"
#include <iostream>
#include <random>
#include <string>
#include <vector>

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

void test_moveToStringPromotions() {
    CHECK(chess::moveToString(chess::Move(chess::A7, chess::A8, chess::KNIGHT)) == "a7a8n");
    CHECK(chess::moveToString(chess::Move(chess::A7, chess::A8, chess::BISHOP)) == "a7a8b");
    CHECK(chess::moveToString(chess::Move(chess::A7, chess::A8, chess::ROOK)) == "a7a8r");
    CHECK(chess::moveToString(chess::Move(chess::A7, chess::A8, chess::QUEEN)) == "a7a8q");
    CHECK(chess::moveToString(chess::Move(chess::E2, chess::E4)) == "e2e4");
}

void test_stringToSquareRejectsInvalid() {
    CHECK(chess::stringToSquare("-") == chess::SQ_NONE);
    CHECK(chess::stringToSquare("") == chess::SQ_NONE);
    CHECK(chess::stringToSquare("i2") == chess::SQ_NONE);
    CHECK(chess::stringToSquare("a9") == chess::SQ_NONE);
}

void test_setFenResetsState() {
    chess::Board b;
    chess::UndoInfo undo;
    b.makeMove(chess::Move(chess::E2, chess::E4), undo);

    b.setFen("4k3/8/8/8/8/8/8/4K3 w - - 12 34");
    CHECK(b.castlingRights() == 0);
    CHECK(b.enPassant() == chess::SQ_NONE);
    CHECK(b.halfMoveClock() == 12);
    CHECK(b.fullMoveNumber() == 34);
    CHECK(b.fen() == "4k3/8/8/8/8/8/8/4K3 w - - 12 34");
}

void test_invalidSetFenDoesNotMutateBoard() {
    chess::Board b;
    std::string start = b.fen();
    uint64_t startHash = b.hash();

    CHECK(!b.setFen("8/8/8/8/8/8/8/8 w - - 0 1"));
    CHECK(b.fen() == start);
    CHECK(b.hash() == startHash);
    CHECK(!b.isRepetition());
}

void test_setFenClearsRepetitionHistory() {
    chess::Board b;
    chess::UndoInfo undo;

    CHECK(b.makeMove(chess::Move(chess::G1, chess::F3), undo));
    CHECK(b.makeMove(chess::Move(chess::G8, chess::F6), undo));
    CHECK(b.makeMove(chess::Move(chess::F3, chess::G1), undo));
    CHECK(b.makeMove(chess::Move(chess::F6, chess::G8), undo));
    CHECK(b.makeMove(chess::Move(chess::G1, chess::F3), undo));
    CHECK(b.makeMove(chess::Move(chess::G8, chess::F6), undo));
    CHECK(b.makeMove(chess::Move(chess::F3, chess::G1), undo));
    CHECK(b.makeMove(chess::Move(chess::F6, chess::G8), undo));
    CHECK(b.isRepetition());

    CHECK(b.setFen(chess::STARTPOS_FEN));
    CHECK(!b.isRepetition());
}

void test_invalidMovesDoNotAffectRepetitionHistory() {
    chess::Board b;
    chess::UndoInfo undo;

    CHECK(!b.makeMove(chess::Move(chess::E2, chess::E5), undo));
    CHECK(!b.makeMove(chess::Move(chess::E2, chess::E5), undo));
    CHECK(!b.isRepetition());
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

void test_invalidSquaresRejectedByBoard() {
    chess::Board b;
    std::string start = b.fen();
    chess::UndoInfo undo;

    CHECK(!b.makeMove(chess::Move(chess::SQ_NONE, chess::E4), undo));
    CHECK(b.fen() == start);
    CHECK(!b.makeMove(chess::Move(chess::E2, chess::SQ_NONE), undo));
    CHECK(b.fen() == start);
}

void test_illegalPseudoMovesRejected() {
    chess::Board b;
    std::string start = b.fen();
    chess::UndoInfo undo;

    CHECK(!b.makeMove(chess::Move(chess::E2, chess::E5), undo));
    CHECK(b.fen() == start);
    CHECK(!b.makeMove(chess::Move(chess::B1, chess::B3), undo));
    CHECK(b.fen() == start);
    CHECK(!b.makeMove(chess::Move(chess::E2, chess::F3), undo));
    CHECK(b.fen() == start);
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

void test_invalidCastlingRejected() {
    {
        chess::Board b;
        std::string start = b.fen();
        CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w K - 0 1"));
        CHECK(b.fen() == start);
    }
    {
        chess::Board b("4k3/8/8/8/8/8/8/4K2R w - - 0 1");
        chess::UndoInfo undo;
        CHECK(!b.makeMove(chess::Move(chess::E1, chess::G1), undo));
        CHECK(b.pieceOn(chess::E1) == chess::W_KING);
        CHECK(b.pieceOn(chess::H1) == chess::W_ROOK);
    }
    {
        chess::Board b;
        std::string start = b.fen();
        CHECK(!b.setFen("4k3/8/8/8/8/8/8/R3K2R w KK - 0 1"));
        CHECK(b.fen() == start);
    }
}

void test_unmakeRestoresCountersAndHash() {
    chess::Board b;
    uint64_t startHash = b.hash();
    chess::UndoInfo whiteUndo;
    CHECK(b.makeMove(chess::Move(chess::E2, chess::E4), whiteUndo));
    uint64_t afterWhiteHash = b.hash();
    int afterWhiteFullMove = b.fullMoveNumber();

    chess::UndoInfo blackUndo;
    CHECK(b.makeMove(chess::Move(chess::E7, chess::E5), blackUndo));
    CHECK(b.fullMoveNumber() == afterWhiteFullMove + 1);
    b.unmakeMove(chess::Move(chess::E7, chess::E5), blackUndo);
    CHECK(b.hash() == afterWhiteHash);
    CHECK(b.fullMoveNumber() == afterWhiteFullMove);

    b.unmakeMove(chess::Move(chess::E2, chess::E4), whiteUndo);
    CHECK(b.hash() == startHash);
    CHECK(b.fullMoveNumber() == 1);
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

void test_kingCaptureRejected() {
    chess::Board b("4k3/8/8/8/4R3/8/8/4K3 w - - 0 1");
    std::string start = b.fen();
    chess::UndoInfo undo;

    CHECK(!b.makeMove(chess::Move(chess::E4, chess::E8), undo));
    CHECK(b.fen() == start);
    CHECK(b.pieceOn(chess::E8) == chess::B_KING);
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

void test_enPassantRequiresCapturedPawn() {
    chess::Board b;
    std::string start = b.fen();
    CHECK(!b.setFen("4k3/8/8/4P3/8/8/8/4K3 w - d6 0 1"));
    CHECK(b.fen() == start);
}

void test_invalidFenFieldsRejected() {
    chess::Board b;
    std::string start = b.fen();
    uint64_t startHash = b.hash();

    CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w - - x y"));
    CHECK(b.fen() == start);
    CHECK(b.hash() == startHash);

    CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w - - 0 0"));
    CHECK(b.fen() == start);

    CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w A - 0 1"));
    CHECK(b.fen() == start);

    CHECK(!b.setFen("8/8/8/8/8/8/4k3/4K3 w - - 0 1"));
    CHECK(b.fen() == start);
}

void test_trailingTokensRejected() {
    chess::Board b;
    std::string start = b.fen();
    uint64_t startHash = b.hash();

    CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w - - 0 1 extra"));
    CHECK(b.fen() == start);
    CHECK(b.hash() == startHash);

    CHECK(!b.setFen("4k3/8/8/8/8/8/8/4K3 w - - 0 1 garbage"));
    CHECK(b.fen() == start);
}

void test_validEnPassantFenRequiresCapturablePawn() {
    chess::Board b;
    CHECK(b.setFen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"));
    CHECK(b.enPassant() == chess::D6);

    std::string start = b.fen();
    chess::UndoInfo undo;
    CHECK(b.makeMove(chess::Move(chess::E5, chess::D6), undo));
    b.unmakeMove(chess::Move(chess::E5, chess::D6), undo);
    CHECK(b.fen() == start);
}

void test_enPassantFenAllowsUncapturableTargetWithoutHashingIt() {
    chess::Board withEp("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    chess::Board withoutEp("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");

    CHECK(withEp.enPassant() == chess::E3);
    CHECK(withEp.fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    CHECK(withEp.hash() == withoutEp.hash());
}

void test_nullMoveClearsCapturableEpHash() {
    chess::Board b("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
    chess::Board expectedNull("4k3/8/8/8/3pP3/8/8/4K3 w - - 1 1");
    uint64_t startHash = b.hash();
    chess::NullUndo undo;

    b.makeNullMove(undo);
    CHECK(b.enPassant() == chess::SQ_NONE);
    CHECK(b.sideToMove() == chess::WHITE);
    CHECK(b.hash() == expectedNull.hash());

    b.unmakeNullMove(undo);
    CHECK(b.hash() == startHash);
    CHECK(b.enPassant() == chess::E3);
    CHECK(b.sideToMove() == chess::BLACK);
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

void test_invalidPromotionsRejected() {
    {
        chess::Board b("8/P7/8/8/8/8/8/k6K w - - 0 1");
        std::string start = b.fen();
        chess::UndoInfo undo;
        CHECK(!b.makeMove(chess::Move(chess::A7, chess::A8), undo));
        CHECK(b.fen() == start);
    }
    {
        chess::Board b("8/P7/8/8/8/8/8/k6K w - - 0 1");
        std::string start = b.fen();
        chess::UndoInfo undo;
        CHECK(!b.makeMove(chess::Move(chess::A7, chess::A8, chess::KING), undo));
        CHECK(b.fen() == start);
    }
    {
        chess::Board b;
        std::string start = b.fen();
        chess::UndoInfo undo;
        CHECK(!b.makeMove(chess::Move(chess::E2, chess::E4, chess::QUEEN), undo));
        CHECK(b.fen() == start);
    }
}

void test_malformedFenHandledSafely() {
    chess::Board badClocks("4k3/8/8/8/8/8/8/4K3 w - - x y");
    CHECK(badClocks.fen() == chess::STARTPOS_FEN);

    chess::Board badPlacement("9/8/8/8/8/8/8/8 w - - 0 1");
    CHECK(badPlacement.fen() == chess::STARTPOS_FEN);
}

void test_impossibleFenOpponentKingAttacked() {
    chess::Board b;
    std::string start = b.fen();

    // White rook on e2 attacks black king on e8 — impossible (R = white rook)
    CHECK(!b.setFen("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1"));
    CHECK(b.fen() == start);

    // White bishop on b5 attacks black king on e8 via diagonal — impossible
    CHECK(!b.setFen("4k3/8/8/1B6/8/8/8/4K3 w - - 0 1"));
    CHECK(b.fen() == start);

    // But a valid checking position (stm's king is in check) is legal
    CHECK(b.setFen("4k3/8/8/8/8/8/3q4/4K3 w - - 0 1"));
}

static bool evalStateEquals(const chess::IncrementalEvalState& a,
                            const chess::IncrementalEvalState& b) {
    return a.material == b.material &&
           a.pstMg == b.pstMg &&
           a.pstEg == b.pstEg &&
           a.phase == b.phase &&
           a.pawnHash == b.pawnHash;
}

static void walkMakeUnmakeInvariants(chess::Board& b, chess::MoveGenerator& gen, int depth) {
    std::string startFen = b.fen();
    uint64_t startHash = b.hash();
    uint64_t startPawnHash = b.pawnHash();
    chess::IncrementalEvalState startEval = b.evalState();

    chess::MoveList moves;
    gen.generateLegalMoves(b, moves);
    for (const chess::Move& m : moves)
        CHECK(b.isMoveLegal(m));

    if (depth == 0)
        return;

    for (const chess::Move& m : moves) {
        chess::UndoInfo undo;
        CHECK(b.makeMove(m, undo));
        walkMakeUnmakeInvariants(b, gen, depth - 1);
        b.unmakeMove(m, undo);

        CHECK(b.fen() == startFen);
        CHECK(b.hash() == startHash);
        CHECK(b.pawnHash() == startPawnHash);
        CHECK(evalStateEquals(b.evalState(), startEval));
    }
}

void test_makeUnmakeInvariantsAcrossGeneratedMoves() {
    std::vector<std::string> fens = {
        chess::STARTPOS_FEN,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
    };

    chess::MoveGenerator gen;
    for (const std::string& fen : fens) {
        chess::Board b(fen);
        walkMakeUnmakeInvariants(b, gen, 2);
    }
}

void test_makeUnmakeInvariantsAcrossPseudoRandomPositions() {
    std::vector<std::string> roots = {
        chess::STARTPOS_FEN,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };

    chess::MoveGenerator gen;
    std::mt19937_64 rng(0xC0FFEEULL);

    for (const std::string& root : roots) {
        chess::Board b(root);
        for (int ply = 0; ply < 48; ply++) {
            walkMakeUnmakeInvariants(b, gen, 1);

            chess::MoveList moves;
            gen.generateLegalMoves(b, moves);
            if (moves.size() == 0)
                break;

            const chess::Move& selected = moves[static_cast<int>(rng() % moves.size())];
            chess::UndoInfo undo;
            CHECK(b.makeMove(selected, undo));
        }
    }
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running board tests:" << std::endl;
    RUN_TEST(startPosition);
    RUN_TEST(fenParsing);
    RUN_TEST(fenRoundtrip);
    RUN_TEST(moveToStringPromotions);
    RUN_TEST(stringToSquareRejectsInvalid);
    RUN_TEST(setFenResetsState);
    RUN_TEST(invalidSetFenDoesNotMutateBoard);
    RUN_TEST(setFenClearsRepetitionHistory);
    RUN_TEST(invalidMovesDoNotAffectRepetitionHistory);
    RUN_TEST(makeMove_basic);
    RUN_TEST(makeMove_doublePush);
    RUN_TEST(makeMove_unmake);
    RUN_TEST(invalidSquaresRejectedByBoard);
    RUN_TEST(illegalPseudoMovesRejected);
    RUN_TEST(castling);
    RUN_TEST(invalidCastlingRejected);
    RUN_TEST(unmakeRestoresCountersAndHash);
    RUN_TEST(illegalSelfCheck);
    RUN_TEST(kingInCheck);
    RUN_TEST(kingCaptureRejected);
    RUN_TEST(enPassantCapture);
    RUN_TEST(enPassantRequiresCapturedPawn);
    RUN_TEST(invalidFenFieldsRejected);
    RUN_TEST(trailingTokensRejected);
    RUN_TEST(validEnPassantFenRequiresCapturablePawn);
    RUN_TEST(enPassantFenAllowsUncapturableTargetWithoutHashingIt);
    RUN_TEST(nullMoveClearsCapturableEpHash);
    RUN_TEST(promotion);
    RUN_TEST(invalidPromotionsRejected);
    RUN_TEST(malformedFenHandledSafely);
    RUN_TEST(impossibleFenOpponentKingAttacked);
    RUN_TEST(makeUnmakeInvariantsAcrossGeneratedMoves);
    RUN_TEST(makeUnmakeInvariantsAcrossPseudoRandomPositions);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll board tests passed." << std::endl;
    return 0;
}
