#include "bot/decision.h"
#include <iostream>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

using namespace chess::bot;

// Default config
static constexpr bool autoResign = true;
static constexpr int resignThreshold = -800;
static constexpr bool autoDraw = true;
static constexpr int drawThreshold = 20;
static constexpr int drawOfferMoves = 4;

void test_resignBelowThreshold() {
    auto d = evaluatePostSearch(false, 0, -900, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::RESIGN);
}

void test_resignAtExactThreshold() {
    // score equals threshold — should NOT resign (strict less-than)
    auto d = evaluatePostSearch(false, 0, -800, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
}

void test_resignAboveThreshold() {
    auto d = evaluatePostSearch(false, 0, -799, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
}

void test_noResignWhenDisabled() {
    auto d = evaluatePostSearch(false, 0, -900, false, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
}

void test_acceptDrawWhenOffered() {
    auto d = evaluatePostSearch(true, 2, 10, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::ACCEPT_DRAW);
}

void test_acceptDrawNearZeroNegative() {
    auto d = evaluatePostSearch(true, 1, -15, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::ACCEPT_DRAW);
}

void test_noAcceptDrawWhenNotOffered() {
    auto d = evaluatePostSearch(false, 2, 5, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
}

void test_noAcceptDrawWhenAhead() {
    // opponent offered draw but we're winning — decline by not accepting
    auto d = evaluatePostSearch(true, 0, 300, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
}

void test_consecutiveCounterIncrements() {
    auto d = evaluatePostSearch(false, 2, 10, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
    CHECK(d.newConsecutiveDrawishMoves == 3);
}

void test_consecutiveCounterResets() {
    auto d = evaluatePostSearch(false, 3, 50, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
    CHECK(d.newConsecutiveDrawishMoves == 0);
}

void test_offerDrawAfterThresholdReached() {
    // 3 previous draw-ish moves, this 4th triggers the offer
    auto d = evaluatePostSearch(false, 3, 10, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::OFFER_DRAW);
    CHECK(d.offeringDraw == true);
    CHECK(d.newConsecutiveDrawishMoves == 0);
}

void test_offerDrawAfterExceedingThreshold() {
    // counter already beyond threshold (e.g., due to config change)
    auto d = evaluatePostSearch(false, 5, -10, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::OFFER_DRAW);
    CHECK(d.offeringDraw == true);
}

void test_noDrawWhenDisabled() {
    // score near zero, opponent offered — but autoDraw is off
    auto d = evaluatePostSearch(true, 2, 5, autoResign, resignThreshold, false, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
    CHECK(d.offeringDraw == false);
}

void test_noOfferWhenDrawDisabled() {
    // enough consecutive draw-ish moves but autoDraw off
    auto d = evaluatePostSearch(false, 4, 5, autoResign, resignThreshold, false, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
    CHECK(d.offeringDraw == false);
}

void test_drawThresholdEdgeCase() {
    // abs(score) == threshold — should trigger draw behavior (< is used, so strict)
    auto d = evaluatePostSearch(false, 0, 20, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::MAKE_MOVE);
    CHECK(d.newConsecutiveDrawishMoves == 0);
}

void test_resignTakesPriorityOverDraw() {
    // score below resign threshold, opponent offered draw — resign wins
    auto d = evaluatePostSearch(true, 3, -900, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::RESIGN);
}

void test_drawOfferResetsCounter() {
    // after offering draw, counter is reset to 0
    auto d = evaluatePostSearch(false, 4, 10, autoResign, resignThreshold, autoDraw, drawThreshold, drawOfferMoves);
    CHECK(d.action == PostSearchAction::OFFER_DRAW);
    CHECK(d.newConsecutiveDrawishMoves == 0);
}

void test_acceptDrawNearZeroMultipleThresholdMoves() {
    // opponent offered, we're slightly below threshold
    auto d = evaluatePostSearch(true, 1, -19, autoResign, resignThreshold, autoDraw, 20, drawOfferMoves);
    CHECK(d.action == PostSearchAction::ACCEPT_DRAW);
}

int main() {
    std::cout << "bot decision tests:" << std::endl;

    RUN_TEST(resignBelowThreshold);
    RUN_TEST(resignAtExactThreshold);
    RUN_TEST(resignAboveThreshold);
    RUN_TEST(noResignWhenDisabled);
    RUN_TEST(acceptDrawWhenOffered);
    RUN_TEST(acceptDrawNearZeroNegative);
    RUN_TEST(noAcceptDrawWhenNotOffered);
    RUN_TEST(noAcceptDrawWhenAhead);
    RUN_TEST(consecutiveCounterIncrements);
    RUN_TEST(consecutiveCounterResets);
    RUN_TEST(offerDrawAfterThresholdReached);
    RUN_TEST(offerDrawAfterExceedingThreshold);
    RUN_TEST(noDrawWhenDisabled);
    RUN_TEST(noOfferWhenDrawDisabled);
    RUN_TEST(drawThresholdEdgeCase);
    RUN_TEST(resignTakesPriorityOverDraw);
    RUN_TEST(drawOfferResetsCounter);
    RUN_TEST(acceptDrawNearZeroMultipleThresholdMoves);

    if (failures) {
        std::cerr << failures << " test(s) FAILED" << std::endl;
        return 1;
    }
    std::cout << "All tests passed." << std::endl;
    return 0;
}
