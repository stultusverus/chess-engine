#pragma once

#include "engine/search.h"
#include <cstdlib>

namespace chess {
namespace bot {

enum class PostSearchAction : int {
    MAKE_MOVE = 0,
    RESIGN,
    ACCEPT_DRAW,
    OFFER_DRAW
};

struct PostSearchDecision {
    PostSearchAction action = PostSearchAction::MAKE_MOVE;
    bool offeringDraw = false;
    int newConsecutiveDrawishMoves = 0;
};

inline PostSearchDecision evaluatePostSearch(
    bool drawOffered,
    int consecutiveDrawishMoves,
    int score,
    bool autoResign, int resignThreshold,
    bool autoDraw, int drawEvalThreshold, int drawOfferMoves)
{
    PostSearchDecision d;
    d.newConsecutiveDrawishMoves = consecutiveDrawishMoves;

    if (autoResign && score < resignThreshold) {
        d.action = PostSearchAction::RESIGN;
        return d;
    }

    if (autoDraw) {
        if (std::abs(score) < drawEvalThreshold) {
            d.newConsecutiveDrawishMoves = consecutiveDrawishMoves + 1;
        } else {
            d.newConsecutiveDrawishMoves = 0;
        }

        if (drawOffered && std::abs(score) < drawEvalThreshold) {
            d.action = PostSearchAction::ACCEPT_DRAW;
            return d;
        }

        if (d.newConsecutiveDrawishMoves >= drawOfferMoves) {
            d.action = PostSearchAction::OFFER_DRAW;
            d.offeringDraw = true;
            d.newConsecutiveDrawishMoves = 0;
            return d;
        }
    }

    return d;
}

} // namespace bot
} // namespace chess
