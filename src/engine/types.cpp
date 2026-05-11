#include "types.h"
#include <string>

namespace chess {

std::string squareToString(Square s) {
    if (s == SQ_NONE) return "-";
    char f = 'a' + fileOf(s);
    char r = '1' + rankOf(s);
    return std::string(1, f) + r;
}

Square stringToSquare(const std::string& s) {
    if (s == "-" || s.size() < 2) return SQ_NONE;
    if (s[0] < 'a' || s[0] > 'h') return SQ_NONE;
    if (s[1] < '1' || s[1] > '8') return SQ_NONE;
    File f = File(s[0] - 'a');
    Rank r = Rank(s[1] - '1');
    return makeSquare(f, r);
}

std::string moveToString(Move m) {
    std::string result = squareToString(m.from) + squareToString(m.to);
    switch (m.promotion) {
    case KNIGHT: result += 'n'; break;
    case BISHOP: result += 'b'; break;
    case ROOK: result += 'r'; break;
    case QUEEN: result += 'q'; break;
    default: break;
    }
    return result;
}

PieceType charToPieceType(char c) {
    switch (c) {
    case 'n': case 'N': return KNIGHT;
    case 'b': case 'B': return BISHOP;
    case 'r': case 'R': return ROOK;
    case 'q': case 'Q': return QUEEN;
    default: return PIECE_TYPE_NB;
    }
}

} // namespace chess
