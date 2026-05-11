#include "book.h"
#include "board.h"
#include "poly_keys.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

namespace chess {

// --- Polyglot piece mapping ---
// Polyglot piece index = (pieceType * 2) + (1 - color)
//   pieceType: PAWN=0, KNIGHT=1, BISHOP=2, ROOK=3, QUEEN=4, KING=5
//   color: WHITE=0, BLACK=1
//   Result: BP=0, WP=1, BN=2, WN=3, BB=4, WB=5, BR=6, WR=7, BQ=8, WQ=9, BK=10, WK=11
static int polyPieceIndex(Piece p) {
    return int(typeOf(p)) * 2 + (int(colorOf(p)) ^ 1);
}

uint64_t Book::polyglotHash(const Board& board) {
    uint64_t hash = 0;

    // Piece keys: random64[pieceIndex * 64 + square]
    for (Square s = A1; s <= H8; s = Square(s + 1)) {
        Piece p = board.pieceOn(s);
        if (p != NO_PIECE) {
            hash ^= polyglot::random64[polyPieceIndex(p) * 64 + int(s)];
        }
    }

    // Castling keys: [768] WK, [769] WQ, [770] BK, [771] BQ
    int cr = board.castlingRights();
    if (cr & WK) hash ^= polyglot::random64[768];
    if (cr & WQ) hash ^= polyglot::random64[769];
    if (cr & BK) hash ^= polyglot::random64[770];
    if (cr & BQ) hash ^= polyglot::random64[771];

    // En passant key: [772 + file], but only if a pawn can actually capture
    Square ep = board.enPassant();
    if (ep != SQ_NONE) {
        Color us = board.sideToMove();
        int epFile = fileOf(ep);
        Bitboard epMask = 0;
        // Check adjacent files for a pawn of the moving side
        if (epFile > FILE_A) epMask |= squareBb(Square(ep + (us == WHITE ? -9 : 7)));
        if (epFile < FILE_H) epMask |= squareBb(Square(ep + (us == WHITE ? -7 : 9)));
        if (epMask & board.pieces(us, PAWN)) {
            hash ^= polyglot::random64[772 + epFile];
        }
    }

    // Side to move key: [780] if WHITE to move
    if (board.sideToMove() == WHITE)
        hash ^= polyglot::random64[780];

    return hash;
}

Move Book::decodePolyglotMove(uint16_t packed) {
    // Polyglot move encoding:
    //   bits 0-5:  to square
    //   bits 6-11: from square
    //   bits 12-14: promotion piece (0=none, 1=knight, 2=bishop, 3=rook, 4=queen)
    Square to = Square(packed & 63);
    Square from = Square((packed >> 6) & 63);
    int promo = (packed >> 12) & 7;

    PieceType pt = PIECE_TYPE_NB;
    if (promo >= KNIGHT && promo <= QUEEN)
        pt = PieceType(promo);

    return Move(from, to, pt);
}

static Move fixupPolyglotMove(Move m) {
    // Polyglot encodes castling as king-to-rook (e1h1, e1a1, e8h8, e8a8)
    // UCI uses king destination (e1g1, e1c1, e8g8, e8c8)
    if (m.from == E1 && m.to == H1) return Move(E1, G1);
    if (m.from == E1 && m.to == A1) return Move(E1, C1);
    if (m.from == E8 && m.to == H8) return Move(E8, G8);
    if (m.from == E8 && m.to == A8) return Move(E8, C8);
    return m;
}

static uint64_t bswap64(uint64_t x) {
    return __builtin_bswap64(x);
}

static uint16_t bswap16(uint16_t x) {
    return __builtin_bswap16(x);
}

static uint32_t bswap32(uint32_t x) {
    return __builtin_bswap32(x);
}

bool Book::load(const std::string& path) {
    entries_.clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[book] Cannot open: " << path << std::endl;
        return false;
    }

    size_t fileSize = file.tellg();
    if (fileSize % 16 != 0) {
        std::cerr << "[book] Corrupt book file (size not multiple of 16)" << std::endl;
        return false;
    }

    size_t entryCount = fileSize / 16;
    entries_.resize(entryCount);

    file.seekg(0, std::ios::beg);

    // Bulk-read entire file in one shot
    std::vector<char> buf(fileSize);
    file.read(buf.data(), static_cast<std::streamsize>(fileSize));

    const char* p = buf.data();
    for (size_t i = 0; i < entryCount; i++, p += 16) {
        Entry& e = entries_[i];

        uint64_t key;
        uint16_t move, weight;
        uint32_t learn;
        std::memcpy(&key, p, 8);
        std::memcpy(&move, p + 8, 2);
        std::memcpy(&weight, p + 10, 2);
        std::memcpy(&learn, p + 12, 4);

        e.key = bswap64(key);
        e.move = bswap16(move);
        e.weight = bswap16(weight);
        e.learn = bswap32(learn);
    }

    // Verify book is sorted by key
    bool sorted = true;
    for (size_t i = 1; i < entryCount; i++) {
        if (entries_[i].key < entries_[i - 1].key) {
            sorted = false;
            break;
        }
    }
    if (!sorted) {
        std::cerr << "[book] Book is not sorted, sorting now..." << std::endl;
        std::sort(entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) { return a.key < b.key; });
    }

    std::cerr << "[book] Loaded " << entryCount << " entries from " << path << std::endl;
    return true;
}

std::optional<Move> Book::probe(const Board& board) const {
    if (!isLoaded()) return std::nullopt;

    // Don't use book beyond maxHalfMoves (default: 10 plies = 20 half-moves)
    int halfMovesPlayed = 2 * (board.fullMoveNumber() - 1) + (board.sideToMove() == WHITE ? 0 : 1);
    if (halfMovesPlayed >= maxHalfMoves_)
        return std::nullopt;

    uint64_t hash = polyglotHash(board);

    // Binary search for first occurrence of the key
    auto it = std::lower_bound(entries_.begin(), entries_.end(), hash,
        [](const Entry& e, uint64_t k) { return e.key < k; });

    // Collect all entries with this key
    std::vector<const Entry*> matches;
    while (it != entries_.end() && it->key == hash) {
        matches.push_back(&(*it));
        ++it;
    }

    if (matches.empty())
        return std::nullopt;

    // Weighted random selection
    uint64_t totalWeight = 0;
    for (const auto* e : matches)
        totalWeight += e->weight;

    if (totalWeight == 0)
        return std::nullopt;

    static std::mt19937_64 rng(std::random_device{}());
    uint64_t r = rng() % totalWeight;

    uint64_t cumulative = 0;
    for (const auto* e : matches) {
        cumulative += e->weight;
        if (r < cumulative)
            return fixupPolyglotMove(decodePolyglotMove(e->move));
    }

    // Fallback (shouldn't reach here)
    return fixupPolyglotMove(decodePolyglotMove(matches.back()->move));
}

} // namespace chess
