#include "tt.h"
#include <algorithm>

namespace chess {

void TranspositionTable::setSize(int mb) {
    // Compute number of entries from MB (16 bytes per entry)
    size_t bytes = static_cast<size_t>(mb) * 1024 * 1024;
    size_t entryCount = bytes / sizeof(TTEntry);
    if (entryCount < 1024) entryCount = 1024;

    // Round to power of 2
    size_t pow2 = 1;
    while (pow2 < entryCount) pow2 <<= 1;
    entries_.resize(pow2);

    clear();
}

void TranspositionTable::clear() {
    std::fill(entries_.begin(), entries_.end(), TTEntry{0, 0, 0, 0, 0});
}

const TTEntry* TranspositionTable::probe(uint64_t hash) const {
    if (entries_.empty()) return nullptr;

    size_t idx = hash & (entries_.size() - 1);
    const TTEntry& entry = entries_[idx];

    if (entry.hash == hash)
        return &entry;
    return nullptr;
}

void TranspositionTable::store(uint64_t hash, int16_t score, int8_t depth, Bound bound, Move move) {
    if (entries_.empty()) return;

    size_t idx = hash & (entries_.size() - 1);
    TTEntry& entry = entries_[idx];

    // Always replace (unless same position with lower depth)
    if (entry.hash == hash && entry.depth > depth && bound != Bound::EXACT)
        return;

    entry.hash = hash;
    entry.score = score;
    entry.depth = depth;
    entry.bound = static_cast<uint8_t>(bound);
    entry.move = packMove(move);
}

int TranspositionTable::hashFull() const {
    if (entries_.empty()) return 0;
    int used = 0;
    for (size_t i = 0; i < std::min(entries_.size(), size_t(1000)); i++) {
        if (entries_[i].hash != 0) used++;
    }
    return static_cast<int>(used * 1000 / std::min(entries_.size(), size_t(1000)));
}

uint16_t TranspositionTable::packMove(Move m) {
    if (m.from == SQ_NONE) return 0;
    uint16_t packed = static_cast<uint16_t>(m.from & 63);
    packed |= static_cast<uint16_t>((m.to & 63) << 6);
    if (m.promotion >= KNIGHT && m.promotion <= QUEEN) {
        packed |= static_cast<uint16_t>(m.promotion << 12);
    }
    return packed;
}

Move TranspositionTable::unpackMove(uint16_t packed) {
    if (packed == 0) return Move();
    Square from = Square(packed & 63);
    Square to = Square((packed >> 6) & 63);
    PieceType promo = PieceType((packed >> 12) & 7);
    if (promo >= KNIGHT && promo <= QUEEN)
        return Move(from, to, promo);
    return Move(from, to);
}

} // namespace chess
