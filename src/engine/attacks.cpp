#include "attacks.h"
#include "board.h"
#include <array>
#include <vector>

namespace chess {
namespace attacks {

namespace {

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

// Known-good magic numbers (precomputed, avoid ~8.5s brute-force search at startup)
constexpr uint64_t bishopMagicsKnown[64] = {
    0x1044080881021200, 0x2104902028110,    0x634010a02000000,  0x68c041181100002,
    0x11104000401930,   0x10c1901008800062,  0x88020804140e0000, 0x4c8410250100c09,
    0x200410405022204,  0x1200281004008030,  0x40802004081,      0x10100410d2001200,
    0x1002140420200420, 0x4209810460150180,  0x8030008088205200, 0x400408544422040,
    0x8000519500c00,    0x104001001022c00,   0x83000380a40c088,  0x824002022408020,
    0x802020400a22204,  0x1004400201102100,  0x1001413212500400, 0x3010802200a40548,
    0x210080010021002,  0xa04200804018400,   0x1090b0020e040044, 0x808248020002,
    0x844082024002019,  0x48004122034608,    0x800820484020281,  0x4404008000208400,
    0x4104000840c10,    0x445108000c1008,    0x341000020480,     0x2008c008200a0200,
    0x5040104010810100, 0x4080021020080,     0x81040c00f10120,   0x800820484020281,
    0x86882030000814,   0xa2080305009820,    0x8081004028841000, 0xe800002018024100,
    0x490881050104500,  0x8020020200ca0200,  0x2104902028110,    0xa2080305009820,
    0x88020804140e0000, 0x230c0101080042,    0x50401540008,      0x100000142120000,
    0x600001b04a060000, 0x30900210410200,    0x32505000808052,   0x2104902028110,
    0x4c8410250100c09,  0x400408544422040,   0xc861580480840,    0x4000020000208803,
    0x8210000012020200, 0x684084010020094,   0x200410405022204,  0x1044080881021200,
};

constexpr uint64_t rookMagicsKnown[64] = {
    0x880002010804000,  0x2040100040002000,  0x4080200080100009, 0x1180080010008482,
    0x1280021400080180, 0x200100844410200,   0x3100010002001094, 0x100042090460100,
    0x8008c0022180,     0x40802000804014,    0x21802001801000,   0x808010000800,
    0x1000411000802,    0x60a001850040600,   0xc003402108138,    0x20a0800041000880,
    0x2040018000403184, 0x2000400a300240,    0x6800820040201600, 0x808010000800,
    0x4008080040800,    0xa620808004000200,  0x2001240001169008, 0x8302000040a401,
    0x240400880088020,  0x401c00740201000,   0x400200180100081,  0x10004240080400,
    0x4008080040800,    0x4000480800200,     0x280020080800100,  0x8010030e00208044,
    0x2204000818000e8,  0x6701002082004200,  0x104101002000,     0xa004016000820,
    0x1c10040080800800, 0x800401800200,      0x2800200804100,    0x6040440066000481,
    0x1008840002d8000,  0x50002001c24008,    0x24080c422060010,  0x8002004010220008,
    0x2082080004008080, 0x12000400090100,    0x401020004010100,  0x210284500820004,
    0x1000400038800480, 0x6701002082004200,  0x2020042015004100, 0x10004901209100,
    0x8004000800800480, 0x2003808400220080,  0x408002d001080400, 0x840d000882004100,
    0x40008003006013c1, 0xc844001082011,     0x810084011002001,  0x5008081001050021,
    0x92000804211002,   0x82001008844102,    0x222480208b00104,  0x12481140442,
};

static void buildBishopTable(Square sq, uint64_t magic) {
    Bitboard mask = bishopMasks_[sq];
    auto& table = bishopTable_[sq];

    int bits = popcount(mask);
    int count = 1 << bits;
    int shift = 64 - bits;
    std::fill(table.begin(), table.begin() + 512, Bitboard(0));

    Bitboard subset = 0;
    for (int i = 0; i < count; i++) {
        Bitboard att = sliderAttacksOTF(sq, subset, true);
        int idx = int((subset * magic) >> shift);
        table[idx] = att;
        subset = (subset - mask) & mask;
    }
}

static void buildRookTable(Square sq, uint64_t magic) {
    Bitboard mask = rookMasks_[sq];
    auto& table = rookTable_[sq];

    int bits = popcount(mask);
    int count = 1 << bits;
    int shift = 64 - bits;
    std::fill(table.begin(), table.begin() + 4096, Bitboard(0));

    Bitboard subset = 0;
    for (int i = 0; i < count; i++) {
        Bitboard att = sliderAttacksOTF(sq, subset, false);
        int idx = int((subset * magic) >> shift);
        table[idx] = att;
        subset = (subset - mask) & mask;
    }
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

    // Magic numbers and attack tables (hardcoded, no runtime search)
    for (int s = 0; s < 64; s++) {
        bishopMagics_[s] = bishopMagicsKnown[s];
        buildBishopTable(Square(s), bishopMagicsKnown[s]);
        rookMagics_[s] = rookMagicsKnown[s];
        buildRookTable(Square(s), rookMagicsKnown[s]);
    }
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
