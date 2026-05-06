#pragma once

#include "types.h"
#include <cstdint>
#include <vector>

namespace chess {

enum class Bound : uint8_t {
    NONE = 0,
    EXACT = 1,
    LOWER = 2,
    UPPER = 3,
};

#pragma pack(push, 1)
struct TTEntry {
    uint64_t hash;
    int16_t score;
    int8_t depth;
    uint8_t bound;
    uint16_t move; // packed: from | (to << 6) | (promotion << 12)
    uint16_t padding;
};
#pragma pack(pop)

static_assert(sizeof(TTEntry) == 16, "TTEntry should be 16 bytes");

class TranspositionTable {
public:
    TranspositionTable() = default;

    void setSize(int mb);
    void clear();

    // Returns pointer to entry, nullptr if not found
    const TTEntry* probe(uint64_t hash) const;

    // Store entry (always replace)
    void store(uint64_t hash, int16_t score, int8_t depth, Bound bound, Move move);

    // Pack/Unpack move
    static uint16_t packMove(Move m);
    static Move unpackMove(uint16_t packed);

    int size() const { return static_cast<int>(entries_.size()); }
    int hashFull() const; // Approximate permille usage

private:
    std::vector<TTEntry> entries_;
};

} // namespace chess
