#include "attacks.h"
#include "board.h"
#include <array>
#include <vector>

namespace chess {
namespace attacks {

namespace {

uint64_t prng(uint64_t& state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

// Tables
std::array<Bitboard, 64> bishopMasks_;
std::array<Bitboard, 64> rookMasks_;
std::array<Bitboard, 64> bishopMagics_;
std::array<Bitboard, 64> rookMagics_;
std::array<std::array<Bitboard, 512>, 64> bishopTable_;
std::array<std::array<Bitboard, 4096>, 64> rookTable_;

// Precomputed non-slider attack tables
std::array<Bitboard, 64> knightAttacks_;
std::array<Bitboard, 64> kingAttacks_;
std::array<std::array<Bitboard, 64>, COLOR_NB> pawnAttacks_;

// Generate slider mask (squares that can block, excluding edges)
Bitboard generateSliderMask(Square sq, bool bishop) {
    Bitboard mask = 0;
    int r = rankOf(sq), f = fileOf(sq);

    if (bishop) {
        for (int dr = 1, df = 1; r + dr <= RANK_7 && f + df <= FILE_G; dr++, df++)
            mask |= squareBb(makeSquare(File(f + df), Rank(r + dr)));
        for (int dr = 1, df = -1; r + dr <= RANK_7 && f + df >= FILE_B; dr++, df--)
            mask |= squareBb(makeSquare(File(f + df), Rank(r + dr)));
        for (int dr = -1, df = 1; r + dr >= RANK_2 && f + df <= FILE_G; dr--, df++)
            mask |= squareBb(makeSquare(File(f + df), Rank(r + dr)));
        for (int dr = -1, df = -1; r + dr >= RANK_2 && f + df >= FILE_B; dr--, df--)
            mask |= squareBb(makeSquare(File(f + df), Rank(r + dr)));
    } else {
        for (int dr = 1; r + dr <= RANK_7; dr++)
            mask |= squareBb(makeSquare(File(f), Rank(r + dr)));
        for (int dr = -1; r + dr >= RANK_2; dr--)
            mask |= squareBb(makeSquare(File(f), Rank(r + dr)));
        for (int df = 1; f + df <= FILE_G; df++)
            mask |= squareBb(makeSquare(File(f + df), Rank(r)));
        for (int df = -1; f + df >= FILE_B; df--)
            mask |= squareBb(makeSquare(File(f + df), Rank(r)));
    }
    return mask;
}

// On-the-fly slider attack generation
Bitboard sliderAttacksOTF(Square sq, Bitboard occupied, bool bishop) {
    Bitboard attacks = 0;
    int r = rankOf(sq), f = fileOf(sq);

    if (bishop) {
        for (int dr = 1, df = 1; r + dr <= RANK_8 && f + df <= FILE_H; dr++, df++) {
            Square ts = makeSquare(File(f + df), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int dr = 1, df = -1; r + dr <= RANK_8 && f + df >= FILE_A; dr++, df--) {
            Square ts = makeSquare(File(f + df), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int dr = -1, df = 1; r + dr >= RANK_1 && f + df <= FILE_H; dr--, df++) {
            Square ts = makeSquare(File(f + df), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int dr = -1, df = -1; r + dr >= RANK_1 && f + df >= FILE_A; dr--, df--) {
            Square ts = makeSquare(File(f + df), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
    } else {
        for (int dr = 1; r + dr <= RANK_8; dr++) {
            Square ts = makeSquare(File(f), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int dr = -1; r + dr >= RANK_1; dr--) {
            Square ts = makeSquare(File(f), Rank(r + dr));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int df = 1; f + df <= FILE_H; df++) {
            Square ts = makeSquare(File(f + df), Rank(r));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
        for (int df = -1; f + df >= FILE_A; df--) {
            Square ts = makeSquare(File(f + df), Rank(r));
            attacks |= squareBb(ts);
            if (occupied & squareBb(ts)) break;
        }
    }
    return attacks;
}

// Magic number search for bishop
void findBishopMagic(Square sq) {
    Bitboard& mask = bishopMasks_[sq];
    Bitboard& magic = bishopMagics_[sq];
    auto& table = bishopTable_[sq];

    int bits = popcount(mask);
    int count = 1 << bits;
    std::vector<Bitboard> occupancies(count);
    std::vector<Bitboard> attacks(count);

    Bitboard subset = 0;
    for (int i = 0; i < count; i++) {
        occupancies[i] = subset;
        attacks[i] = sliderAttacksOTF(sq, subset, true);
        subset = (subset - mask) & mask;
    }

    uint64_t seed = bits;
    int shift = 64 - bits;
    for (int attempt = 0; attempt < 10000000; attempt++) {
        uint64_t m = prng(seed) & prng(seed) & prng(seed);
        if (popcount((m * mask) >> 56) < bits / 2) continue;

        std::fill(table.begin(), table.begin() + 512, Bitboard(0));
        bool ok = true;
        for (int i = 0; i < count && ok; i++) {
            int idx = int((occupancies[i] * m) >> shift);
            if (table[idx] == 0)
                table[idx] = attacks[i];
            else if (table[idx] != attacks[i])
                ok = false;
        }
        if (ok) { magic = m; return; }
    }
    magic = 0;
}

// Magic number search for rook
void findRookMagic(Square sq) {
    Bitboard& mask = rookMasks_[sq];
    Bitboard& magic = rookMagics_[sq];
    auto& table = rookTable_[sq];

    int bits = popcount(mask);
    int count = 1 << bits;
    std::vector<Bitboard> occupancies(count);
    std::vector<Bitboard> attacks(count);

    Bitboard subset = 0;
    for (int i = 0; i < count; i++) {
        occupancies[i] = subset;
        attacks[i] = sliderAttacksOTF(sq, subset, false);
        subset = (subset - mask) & mask;
    }

    uint64_t seed = bits + 32;
    int shift = 64 - bits;
    for (int attempt = 0; attempt < 10000000; attempt++) {
        uint64_t m = prng(seed) & prng(seed) & prng(seed);
        if (popcount((m * mask) >> 56) < bits / 2) continue;

        std::fill(table.begin(), table.begin() + 4096, Bitboard(0));
        bool ok = true;
        for (int i = 0; i < count && ok; i++) {
            int idx = int((occupancies[i] * m) >> shift);
            if (table[idx] == 0)
                table[idx] = attacks[i];
            else if (table[idx] != attacks[i])
                ok = false;
        }
        if (ok) { magic = m; return; }
    }
    magic = 0;
}

} // namespace

void init() {
    // Knight attacks
    for (int s = 0; s < 64; s++) {
        Bitboard b = 0;
        int r = s >> 3, f = s & 7;
        const int knightOffsets[8][2] = {
            {2, 1}, {1, 2}, {-1, 2}, {-2, 1},
            {-2, -1}, {-1, -2}, {1, -2}, {2, -1}
        };
        for (auto& o : knightOffsets) {
            int nr = r + o[0], nf = f + o[1];
            if (nr >= RANK_1 && nr <= RANK_8 && nf >= FILE_A && nf <= FILE_H)
                b |= squareBb(makeSquare(File(nf), Rank(nr)));
        }
        knightAttacks_[s] = b;
    }

    // King attacks
    for (int s = 0; s < 64; s++) {
        Bitboard b = 0;
        int r = s >> 3, f = s & 7;
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;
                int nr = r + dr, nf = f + df;
                if (nr >= RANK_1 && nr <= RANK_8 && nf >= FILE_A && nf <= FILE_H)
                    b |= squareBb(makeSquare(File(nf), Rank(nr)));
            }
        }
        kingAttacks_[s] = b;
    }

    // Pawn attacks
    for (int s = 0; s < 64; s++) {
        Bitboard w = 0, b = 0;
        int r = s >> 3, f = s & 7;
        if (r < RANK_8 && f < FILE_H) w |= squareBb(makeSquare(File(f + 1), Rank(r + 1)));
        if (r < RANK_8 && f > FILE_A) w |= squareBb(makeSquare(File(f - 1), Rank(r + 1)));
        if (r > RANK_1 && f < FILE_H) b |= squareBb(makeSquare(File(f + 1), Rank(r - 1)));
        if (r > RANK_1 && f > FILE_A) b |= squareBb(makeSquare(File(f - 1), Rank(r - 1)));
        pawnAttacks_[WHITE][s] = w;
        pawnAttacks_[BLACK][s] = b;
    }

    // Slider masks
    for (int s = 0; s < 64; s++) {
        Square sq = Square(s);
        bishopMasks_[s] = generateSliderMask(sq, true);
        rookMasks_[s] = generateSliderMask(sq, false);
    }

    // Magic numbers and attack tables
    for (int s = 0; s < 64; s++) findBishopMagic(Square(s));
    for (int s = 0; s < 64; s++) findRookMagic(Square(s));
}

// Public attack queries
Bitboard pawnAttacks(Square s, Color c) { return pawnAttacks_[c][s]; }
Bitboard knightAttacks(Square s) { return knightAttacks_[s]; }
Bitboard kingAttacks(Square s) { return kingAttacks_[s]; }

Bitboard bishopAttacks(Square s, Bitboard occupied) {
    Bitboard rel = occupied & bishopMasks_[s];
    return bishopTable_[s][(rel * bishopMagics_[s]) >> (64 - popcount(bishopMasks_[s]))];
}

Bitboard rookAttacks(Square s, Bitboard occupied) {
    Bitboard rel = occupied & rookMasks_[s];
    return rookTable_[s][(rel * rookMagics_[s]) >> (64 - popcount(rookMasks_[s]))];
}

Bitboard queenAttacks(Square s, Bitboard occupied) {
    return bishopAttacks(s, occupied) | rookAttacks(s, occupied);
}

bool isSquareAttacked(const Board& board, Square s, Color by) {
    Bitboard occ = board.occupied();

    if (by == BLACK) {
        if (board.pieces(BLACK, PAWN) & pawnAttacks(s, WHITE)) return true;
    } else {
        if (board.pieces(WHITE, PAWN) & pawnAttacks(s, BLACK)) return true;
    }

    if (board.pieces(by, KNIGHT) & knightAttacks(s)) return true;
    if ((board.pieces(by, BISHOP) | board.pieces(by, QUEEN)) & bishopAttacks(s, occ)) return true;
    if ((board.pieces(by, ROOK) | board.pieces(by, QUEEN)) & rookAttacks(s, occ)) return true;
    if (board.pieces(by, KING) & kingAttacks(s)) return true;

    return false;
}

Bitboard attackersTo(const Board& board, Square s, Bitboard occupied) {
    Bitboard result = 0;
    result |= pawnAttacks(s, BLACK) & board.pieces(WHITE, PAWN);
    result |= pawnAttacks(s, WHITE) & board.pieces(BLACK, PAWN);
    result |= knightAttacks(s) & (board.pieces(WHITE, KNIGHT) | board.pieces(BLACK, KNIGHT));
    result |= bishopAttacks(s, occupied) & (board.pieces(BISHOP) | board.pieces(QUEEN));
    result |= rookAttacks(s, occupied) & (board.pieces(ROOK) | board.pieces(QUEEN));
    result |= kingAttacks(s) & (board.pieces(WHITE, KING) | board.pieces(BLACK, KING));
    return result;
}

} // namespace attacks
} // namespace chess
