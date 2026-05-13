#pragma once

#include "types.h"
#include <array>
#include <cstdint>
#include <vector>

namespace chess {

constexpr int TT_STATIC_EVAL_NONE = 32767;

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
    int16_t staticEval;
    int8_t depth;
    uint8_t metadata; // low 2 bits: Bound, high 6 bits: generation
    uint16_t move; // packed: from | (to << 6) | (promotion << 12)

    int scoreValue() const;
    bool hasStaticEval() const { return staticEval != TT_STATIC_EVAL_NONE; }
    int staticEvalValue() const { return staticEval; }
    Bound bound() const { return static_cast<Bound>(metadata & 0x03); }
    uint8_t generation() const { return metadata >> 2; }
};
#pragma pack(pop)

static_assert(sizeof(TTEntry) == 16, "TTEntry should be 16 bytes");

constexpr int TT_CLUSTER_SIZE = 4;

struct TTCluster {
    std::array<TTEntry, TT_CLUSTER_SIZE> entries;
};

static_assert(sizeof(TTCluster) == TT_CLUSTER_SIZE * sizeof(TTEntry),
              "TTCluster should be tightly packed");

class TranspositionTable {
public:
    TranspositionTable() = default;

    void setSize(int mb);
    void clear();
    void newSearch();

    // Returns pointer to entry, nullptr if not found
    const TTEntry* probe(uint64_t hash) const;

    // Store entry using depth/generation-preferred replacement.
    void store(uint64_t hash, int score, int8_t depth, Bound bound, Move move,
               int staticEval = TT_STATIC_EVAL_NONE);

    // Pack/Unpack move
    static uint16_t packMove(Move m);
    static Move unpackMove(uint16_t packed);

    int size() const { return static_cast<int>(clusters_.size() * TT_CLUSTER_SIZE); }
    uint8_t generation() const { return generation_; }
    int hashFull() const; // Approximate permille usage

private:
    std::vector<TTCluster> clusters_;
    uint8_t generation_ = 0;
};

} // namespace chess
