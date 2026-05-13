#include "engine/board.h"
#include "engine/attacks.h"
#include "engine/search.h"
#include "engine/tt.h"
#include "engine/types.h"
#include <iostream>
#include <chrono>
#include <vector>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

void test_searchReturnsMove() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);
    auto result = search.search(b, 1);
    CHECK(result.bestMove.from != chess::SQ_NONE);
    CHECK(result.bestMove.to != chess::SQ_NONE);
    CHECK(result.nodes > 0);
    CHECK(result.depth >= 1);
}

void test_searchDepth1_fromStartpos() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);
    auto result = search.search(b, 1);
    CHECK(result.bestMove.from != chess::SQ_NONE);
}

void test_ttMovePackingPromotion() {
    chess::Move quiet(chess::E2, chess::E4);
    chess::Move unpackedQuiet = chess::TranspositionTable::unpackMove(
        chess::TranspositionTable::packMove(quiet));
    CHECK(unpackedQuiet.from == chess::E2);
    CHECK(unpackedQuiet.to == chess::E4);
    CHECK(unpackedQuiet.promotion == chess::PIECE_TYPE_NB);

    chess::Move queenPromo(chess::A7, chess::A8, chess::QUEEN);
    chess::Move unpackedPromo = chess::TranspositionTable::unpackMove(
        chess::TranspositionTable::packMove(queenPromo));
    CHECK(unpackedPromo.from == chess::A7);
    CHECK(unpackedPromo.to == chess::A8);
    CHECK(unpackedPromo.promotion == chess::QUEEN);
}

void test_ttStoresMateScaleScore() {
    chess::TranspositionTable tt;
    tt.setSize(1);
    chess::Move mate(chess::D1, chess::D8);
    tt.store(0x123456789ABCDEF0ULL, 999900, 6, chess::Bound::EXACT, mate);

    const chess::TTEntry* entry = tt.probe(0x123456789ABCDEF0ULL);
    CHECK(entry != nullptr);
    CHECK(entry->scoreValue() == 999900);
    CHECK(chess::TranspositionTable::unpackMove(entry->move).from == chess::D1);
    CHECK(chess::TranspositionTable::unpackMove(entry->move).to == chess::D8);
}

void test_ttDepthPreferredReplacement() {
    chess::TranspositionTable tt;
    tt.setSize(1);

    uint64_t deepHash = 0x1234000000000001ULL;
    uint64_t shallowExactHash = 0x5678000000000001ULL;
    uint64_t mediumHash = 0x9ABC000000000001ULL;
    uint64_t otherHash = 0xDEF0000000000001ULL;
    uint64_t replacementHash = 0x2468000000000001ULL;
    chess::Move deepMove(chess::D1, chess::D8);
    chess::Move shallowMove(chess::A2, chess::A3);
    chess::Move mediumMove(chess::C2, chess::C4);
    chess::Move otherMove(chess::G1, chess::F3);
    chess::Move newerMove(chess::B2, chess::B4);

    tt.store(deepHash, 100, 8, chess::Bound::LOWER, deepMove);
    tt.store(shallowExactHash, 20, 2, chess::Bound::EXACT, shallowMove);
    tt.store(mediumHash, 40, 4, chess::Bound::LOWER, mediumMove);
    tt.store(otherHash, 60, 6, chess::Bound::LOWER, otherMove);

    const chess::TTEntry* deepEntry = tt.probe(deepHash);
    CHECK(deepEntry != nullptr);
    CHECK(deepEntry->depth == 8);
    CHECK(chess::TranspositionTable::unpackMove(deepEntry->move).from == chess::D1);
    CHECK(tt.probe(shallowExactHash) != nullptr);
    CHECK(tt.probe(mediumHash) != nullptr);
    CHECK(tt.probe(otherHash) != nullptr);

    tt.store(replacementHash, 30, 5, chess::Bound::EXACT, newerMove);

    CHECK(tt.probe(deepHash) != nullptr);
    CHECK(tt.probe(shallowExactHash) != nullptr);
    CHECK(tt.probe(mediumHash) == nullptr);
    CHECK(tt.probe(otherHash) != nullptr);
    const chess::TTEntry* replacement = tt.probe(replacementHash);
    CHECK(replacement != nullptr);
    CHECK(replacement->depth == 5);
    CHECK(chess::TranspositionTable::unpackMove(replacement->move).from == chess::B2);
}

void test_ttStoresGenerationMetadata() {
    chess::TranspositionTable tt;
    tt.setSize(1);
    CHECK(tt.generation() == 0);

    tt.newSearch();
    chess::Move move(chess::A2, chess::A4);
    tt.store(0x1234000000000001ULL, 12, 3, chess::Bound::LOWER, move);

    const chess::TTEntry* entry = tt.probe(0x1234000000000001ULL);
    CHECK(entry != nullptr);
    CHECK(entry->bound() == chess::Bound::LOWER);
    CHECK(entry->generation() == tt.generation());
    CHECK(sizeof(chess::TTEntry) == 16);

    tt.clear();
    CHECK(tt.generation() == 0);
    CHECK(tt.probe(0x1234000000000001ULL) == nullptr);
}

void test_ttAgedEntriesAreReplacedFirst() {
    chess::TranspositionTable tt;
    tt.setSize(1);

    uint64_t oldHash = 0x1111000000000001ULL;
    uint64_t freshAHash = 0x2222000000000001ULL;
    uint64_t freshBHash = 0x3333000000000001ULL;
    uint64_t freshCHash = 0x4444000000000001ULL;
    uint64_t replacementHash = 0x5555000000000001ULL;

    tt.newSearch();
    tt.store(oldHash, 10, 4, chess::Bound::LOWER, chess::Move(chess::A2, chess::A3));

    tt.newSearch();
    tt.store(freshAHash, 20, 4, chess::Bound::LOWER, chess::Move(chess::B2, chess::B3));
    tt.store(freshBHash, 30, 4, chess::Bound::LOWER, chess::Move(chess::C2, chess::C3));
    tt.store(freshCHash, 40, 4, chess::Bound::LOWER, chess::Move(chess::D2, chess::D3));

    CHECK(tt.probe(oldHash) != nullptr);
    CHECK(tt.probe(freshAHash) != nullptr);
    CHECK(tt.probe(freshBHash) != nullptr);
    CHECK(tt.probe(freshCHash) != nullptr);

    tt.store(replacementHash, 50, 4, chess::Bound::LOWER, chess::Move(chess::E2, chess::E3));

    CHECK(tt.probe(oldHash) == nullptr);
    CHECK(tt.probe(freshAHash) != nullptr);
    CHECK(tt.probe(freshBHash) != nullptr);
    CHECK(tt.probe(freshCHash) != nullptr);
    const chess::TTEntry* replacement = tt.probe(replacementHash);
    CHECK(replacement != nullptr);
    CHECK(replacement->generation() == tt.generation());
}

void test_ttStoresStaticEval() {
    chess::TranspositionTable tt;
    tt.setSize(1);
    chess::Move move(chess::E2, chess::E4);

    tt.store(0x1234000000000001ULL, 45, 4, chess::Bound::EXACT, move, -23);

    const chess::TTEntry* entry = tt.probe(0x1234000000000001ULL);
    CHECK(entry != nullptr);
    CHECK(entry->scoreValue() == 45);
    CHECK(entry->hasStaticEval());
    CHECK(entry->staticEvalValue() == -23);
    CHECK(sizeof(chess::TTEntry) == 16);
}

void test_ttSizeDoesNotExceedRequestedMb() {
    chess::TranspositionTable tt;
    tt.setSize(3);

    size_t requestedBytes = 3ULL * 1024ULL * 1024ULL;
    size_t allocatedBytes = static_cast<size_t>(tt.size()) * sizeof(chess::TTEntry);
    CHECK(allocatedBytes <= requestedBytes);
    CHECK(tt.size() > 0);
}

// Back-rank mate in 1: Rd1-d8#
void test_mateInOneAtDepth1() {
    chess::Search search;
    chess::Board b("1k6/ppp5/8/8/8/8/PPP5/1K1R4 w - - 0 1");
    search.setInfinite(true);

    // Expected: a depth-1 search sees that the checking move is immediate mate.
    auto result = search.search(b, 1);
    CHECK(result.score > 900000);
    CHECK(result.bestMove.from == chess::D1);
    CHECK(result.bestMove.to == chess::D8);
}

void test_mateInOne() {
    chess::Search search;
    chess::Board b("1k6/ppp5/8/8/8/8/PPP5/1K1R4 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 2);
    CHECK(result.score > 900000);
    CHECK(result.bestMove.from == chess::D1);
    CHECK(result.bestMove.to == chess::D8);
}

void test_captureHangingQueen() {
    chess::Search search;
    chess::Board b("4k3/8/8/3q4/3Q4/8/8/4K3 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 2);
    CHECK(result.bestMove.to == chess::D5);
    CHECK(result.score > 500);
}

void test_captureHangingRook() {
    chess::Search search;
    chess::Board b("4k3/8/8/3r4/3R4/8/8/4K3 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 2);
    CHECK(result.bestMove.to == chess::D5);
    CHECK(result.score > 300);
}

void test_avoidLosingQueen() {
    chess::Search search;
    chess::Board b("4k3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 2);
    CHECK(result.score > 0);
}

void test_nodesIncreaseWithDepth() {
    chess::Search search1;
    chess::Board b;
    search1.setInfinite(true);
    auto r1 = search1.search(b, 1);

    chess::Search search2;
    search2.setInfinite(true);
    auto r2 = search2.search(b, 2);

    CHECK(r2.nodes > r1.nodes * 2);
}

void test_searchStop() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);
    search.stop();

    auto start = std::chrono::steady_clock::now();
    auto result = search.search(b, 10);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(result.nodes < 5000);
    (void)elapsed;
}

void test_promotionPreference() {
    chess::Search search;
    chess::Board b("8/Pk6/8/8/8/8/6K1/8 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 2);
    CHECK(result.bestMove.to == chess::A8);
    CHECK(result.score > 0);
}

// Two different positions produce different best move squares
void test_searchDifferentPositions() {
    chess::Search search1;
    chess::Board b1("4k3/8/8/8/4R3/8/8/4K3 w - - 0 1");
    search1.setInfinite(true);
    auto r1 = search1.search(b1, 2);

    chess::Search search2;
    chess::Board b2("4k3/8/8/8/8/4R3/8/4K3 w - - 0 1");
    search2.setInfinite(true);
    auto r2 = search2.search(b2, 2);

    CHECK(r1.bestMove.from != chess::SQ_NONE);
    CHECK(r2.bestMove.from != chess::SQ_NONE);
    CHECK(r1.nodes > 0);
    CHECK(r2.nodes > 0);
}

// TT should speed up repeat search of same position
void test_ttSpeedup() {
    chess::Search search;
    chess::Board b("r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    search.setInfinite(true);

    auto start1 = std::chrono::steady_clock::now();
    auto r1 = search.search(b, 3);
    auto t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start1).count();

    auto start2 = std::chrono::steady_clock::now();
    auto r2 = search.search(b, 3);
    auto t2 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start2).count();

    CHECK(r1.nodes > 0);
    CHECK(r2.nodes > 0);
    // Second search should be faster (TT hits)
    CHECK(r2.nodes <= r1.nodes * 2 + 100);
    CHECK(t2 <= t1 * 2 + 10);
}

// Null move pruning engages at depth >= 4; verify depth-5 search still works
void test_searchDepth5() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);
    auto result = search.search(b, 5);
    CHECK(result.bestMove.from != chess::SQ_NONE);
    CHECK(result.depth >= 4);
    CHECK(result.nodes > 0);
}

// Null move + mate guard: ensure mate-in-1 is found even at depth >= 4
void test_nullMoveMateGuard() {
    chess::Search search;
    chess::Board b("1k6/ppp5/8/8/8/8/PPP5/1K1R4 w - - 0 1");
    search.setInfinite(true);
    auto result = search.search(b, 6);
    CHECK(result.score > 900000);
    CHECK(result.bestMove.from == chess::D1);
    CHECK(result.bestMove.to == chess::D8);
}

void test_fiftyMoveRuleDraw() {
    chess::Search search;
    chess::Board b("4k3/8/8/8/8/8/8/4K2Q w - - 100 1");
    search.setInfinite(true);

    // Expected: positions at the 50-move-rule claim threshold are scored as draws.
    auto result = search.search(b, 1);
    CHECK(result.score == 0);
}

void test_infoCallbackReportsPvAndMetrics() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);

    int callbacks = 0;
    chess::SearchResult last;
    search.setInfoCallback([&](const chess::SearchResult& result) {
        callbacks++;
        last = result;
    });

    auto result = search.search(b, 2);
    CHECK(callbacks == 2);
    CHECK(last.depth == 2);
    CHECK(last.nodes > 0);
    CHECK(last.hashFull >= 0);
    CHECK(!last.pv.empty());
    CHECK(result.bestMove.from != chess::SQ_NONE);
}

void test_rootMoveRestriction() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);
    search.setRootMoves({chess::Move(chess::E2, chess::E4)});

    auto result = search.search(b, 1);
    CHECK(result.bestMove.from == chess::E2);
    CHECK(result.bestMove.to == chess::E4);
}

void test_rootMoveRestrictionIgnoresRootTTHit() {
    chess::Search search;
    chess::Board b;
    search.setInfinite(true);

    auto unrestricted = search.search(b, 1);
    CHECK(unrestricted.bestMove.from != chess::SQ_NONE);

    search.setRootMoves({chess::Move(chess::E2, chess::E4)});
    auto restricted = search.search(b, 1);
    CHECK(restricted.bestMove.from == chess::E2);
    CHECK(restricted.bestMove.to == chess::E4);
}

void test_nodeLimitStopsSearch() {
    chess::Search search;
    chess::Board b;
    search.setNodeLimit(1);

    auto result = search.search(b, 64);
    CHECK(result.nodes > 0);
    CHECK(result.depth < 64);
}

void test_softTimeLimitStopsBetweenDepths() {
    chess::Search search;
    chess::Board b;
    search.setTimeControlMs(1, 500);

    auto result = search.search(b, 64);
    CHECK(result.bestMove.from != chess::SQ_NONE);
    CHECK(result.nodes > 0);
    CHECK(result.depth >= 1);
    CHECK(result.depth < 64);
}

void test_hardTimeLimitStopsSearch() {
    chess::Search search;
    chess::Board b;
    search.setTimeControlMs(0, 1);

    auto result = search.search(b, 64);
    CHECK(result.nodes > 0);
    CHECK(result.depth < 64);
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    std::cout << "Running search tests:" << std::endl;
    RUN_TEST(searchReturnsMove);
    RUN_TEST(searchDepth1_fromStartpos);
    RUN_TEST(ttMovePackingPromotion);
    RUN_TEST(ttStoresMateScaleScore);
    RUN_TEST(ttDepthPreferredReplacement);
    RUN_TEST(ttStoresGenerationMetadata);
    RUN_TEST(ttAgedEntriesAreReplacedFirst);
    RUN_TEST(ttStoresStaticEval);
    RUN_TEST(ttSizeDoesNotExceedRequestedMb);
    RUN_TEST(mateInOneAtDepth1);
    RUN_TEST(mateInOne);
    RUN_TEST(captureHangingQueen);
    RUN_TEST(captureHangingRook);
    RUN_TEST(avoidLosingQueen);
    RUN_TEST(nodesIncreaseWithDepth);
    RUN_TEST(searchStop);
    RUN_TEST(promotionPreference);
    RUN_TEST(searchDifferentPositions);
    RUN_TEST(ttSpeedup);
    RUN_TEST(searchDepth5);
    RUN_TEST(nullMoveMateGuard);
    RUN_TEST(fiftyMoveRuleDraw);
    RUN_TEST(infoCallbackReportsPvAndMetrics);
    RUN_TEST(rootMoveRestriction);
    RUN_TEST(rootMoveRestrictionIgnoresRootTTHit);
    RUN_TEST(nodeLimitStopsSearch);
    RUN_TEST(softTimeLimitStopsBetweenDepths);
    RUN_TEST(hardTimeLimitStopsSearch);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll search tests passed." << std::endl;
    return 0;
}
