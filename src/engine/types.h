#pragma once

#include <cstdint>
#include <string>

namespace chess {

using Bitboard = uint64_t;

// --- Squares ---
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = 64
};

enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB = 8 };
enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB = 8 };

// --- Colors ---
enum Color : int { WHITE, BLACK, COLOR_NB = 2 };

constexpr Color operator~(Color c) { return Color(c ^ 1); }

// --- Piece types ---
enum PieceType : int {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_TYPE_NB = 6
};

// --- Pieces (color + type) ---
enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 12,
    NO_PIECE = 12
};

// --- Move ---
enum MoveType : int {
    NORMAL = 0,
    CAPTURE,
    EN_PASSANT,
    CASTLING,
    PROMOTION,
    PROMOTION_CAPTURE
};

struct Move {
    Square from = SQ_NONE;
    Square to = SQ_NONE;
    PieceType promotion = PIECE_TYPE_NB;
    MoveType type = NORMAL;

    Move() = default;
    Move(Square from_, Square to_, PieceType promo = PIECE_TYPE_NB, MoveType type_ = NORMAL)
        : from(from_), to(to_), promotion(promo), type(type_) {}
};

// --- Castling ---
enum CastlingRights : int {
    WK = 1, WQ = 2, BK = 4, BQ = 8,
    ALL = 15
};

// --- Constants ---
constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY = 128;
constexpr const char* STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// --- Piece helpers ---
constexpr Piece makePiece(Color c, PieceType pt) { return Piece(c * PIECE_TYPE_NB + pt); }
constexpr PieceType typeOf(Piece p) { return PieceType(p % PIECE_TYPE_NB); }
constexpr Color colorOf(Piece p) { return Color(p / PIECE_TYPE_NB); }

// --- Square helpers ---
constexpr File fileOf(Square s) { return File(s & 7); }
constexpr Rank rankOf(Square s) { return Rank(s >> 3); }
constexpr Square makeSquare(File f, Rank r) { return Square((r << 3) | f); }
constexpr int squareDistance(Square a, Square b) {
    int df = fileOf(a) - fileOf(b);
    int dr = rankOf(a) - rankOf(b);
    return df * df + dr * dr;
}

// --- Bitboard helpers ---
constexpr Bitboard squareBb(Square s) { return 1ULL << s; }

inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(b);
#else
    b -= (b >> 1) & 0x5555555555555555ULL;
    b = (b & 0x3333333333333333ULL) + ((b >> 2) & 0x3333333333333333ULL);
    b = (b + (b >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (b * 0x0101010101010101ULL) >> 56;
#endif
}

inline Square lsb(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return Square(__builtin_ctzll(b));
#else
    static const int index64[64] = {
         0, 47,  1, 56, 48, 27,  2, 60,
        57, 49, 41, 37, 28, 16,  3, 61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11,  4, 62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30,  9, 24,
        13, 18,  8, 12,  7,  6,  5, 63
    };
    return Square(index64[((b ^ (b - 1)) * 0x03F79D71B4CB0A89ULL) >> 58]);
#endif
}

inline Square popLsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// --- Direction shifts ---
constexpr Bitboard NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;
constexpr Bitboard NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;
constexpr Bitboard NOT_AB_FILE = 0xFCFCFCFCFCFCFCFCULL;
constexpr Bitboard NOT_GH_FILE = 0x3F3F3F3F3F3F3F3FULL;

inline Bitboard shiftN (Bitboard b) { return b << 8; }
inline Bitboard shiftS (Bitboard b) { return b >> 8; }
inline Bitboard shiftE (Bitboard b) { return (b & NOT_H_FILE) << 1; }
inline Bitboard shiftW (Bitboard b) { return (b & NOT_A_FILE) >> 1; }
inline Bitboard shiftNE(Bitboard b) { return (b & NOT_H_FILE) << 9; }
inline Bitboard shiftNW(Bitboard b) { return (b & NOT_A_FILE) << 7; }
inline Bitboard shiftSE(Bitboard b) { return (b & NOT_H_FILE) >> 7; }
inline Bitboard shiftSW(Bitboard b) { return (b & NOT_A_FILE) >> 9; }

// --- String conversions ---
std::string squareToString(Square s);
std::string moveToString(Move m);
Square stringToSquare(const std::string& s);
PieceType charToPieceType(char c);

} // namespace chess
