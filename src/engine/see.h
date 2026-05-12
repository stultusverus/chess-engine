#pragma once

#include "types.h"

namespace chess {

class Board;

// Static exchange evaluation from the side-to-move perspective.
// Positive values mean the initiating move wins material after optimal recaptures.
int staticExchangeEval(const Board& board, Move move);

} // namespace chess
