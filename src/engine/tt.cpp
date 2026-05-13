#include "tt.h"

namespace chess {
namespace {

constexpr int EXACT_BOUND_BONUS = 8;
constexpr int AGE_PENALTY = 4;
constexpr uint8_t TT_BOUND_MASK = 0x03;
constexpr uint8_t TT_GENERATION_MASK = 0x3F;

int replacementPriority(int8_t depth, Bound bound) {
    return int(depth) * 2 + (bound == Bound::EXACT ? EXACT_BOUND_BONUS : 0);
}

int entryAge(const TTEntry& entry, uint8_t currentGeneration) {
    return (currentGeneration + TT_GENERATION_MASK + 1 - entry.generation()) &
           TT_GENERATION_MASK;
}

int replacementPriority(const TTEntry& entry, uint8_t currentGeneration) {
    return replacementPriority(entry.depth, entry.bound()) -
           entryAge(entry, currentGeneration) * AGE_PENALTY;
}

uint8_t makeMetadata(Bound bound, uint8_t generation) {
    return static_cast<uint8_t>(((generation & TT_GENERATION_MASK) << 2) |
                                (static_cast<uint8_t>(bound) & TT_BOUND_MASK));
}

void writeEntry(TTEntry& entry, uint64_t hash, int score, int8_t depth,
                Bound bound, Move move, uint8_t generation) {
    entry.hash = hash;
    entry.score = score;
    entry.depth = depth;
    entry.metadata = makeMetadata(bound, generation);
    entry.move = TranspositionTable::packMove(move);
}

} // namespace

void TranspositionTable::setSize(int mb) {
    if (mb < 1) mb = 1;

    // Compute number of entries from MB (16 bytes per entry)
    size_t bytes = static_cast<size_t>(mb) * 1024 * 1024;
    size_t entryCount = bytes / sizeof(TTEntry);
    if (entryCount < 1024) entryCount = 1024;
    size_t clusterCount = entryCount / TT_CLUSTER_SIZE;
    if (clusterCount < 1) clusterCount = 1;

    // Round down so the UCI Hash option remains an allocation cap.
    size_t pow2 = 1;
    while (pow2 <= clusterCount / 2) pow2 <<= 1;
    clusters_.resize(pow2);

    clear();
}

void TranspositionTable::clear() {
    generation_ = 0;
    for (TTCluster& cluster : clusters_)
        cluster.entries.fill(TTEntry{0, 0, 0, 0, 0});
}

void TranspositionTable::newSearch() {
    generation_ = (generation_ + 1) & TT_GENERATION_MASK;
}

const TTEntry* TranspositionTable::probe(uint64_t hash) const {
    if (clusters_.empty()) return nullptr;

    size_t idx = hash & (clusters_.size() - 1);
    const TTCluster& cluster = clusters_[idx];
    for (const TTEntry& entry : cluster.entries) {
        if (entry.hash == hash)
            return &entry;
    }
    return nullptr;
}

void TranspositionTable::store(uint64_t hash, int score, int8_t depth, Bound bound, Move move) {
    if (clusters_.empty()) return;

    size_t idx = hash & (clusters_.size() - 1);
    TTCluster& cluster = clusters_[idx];

    for (TTEntry& entry : cluster.entries) {
        if (entry.hash != hash)
            continue;
        if (entry.depth > depth && bound != Bound::EXACT) {
            entry.metadata = makeMetadata(entry.bound(), generation_);
            return;
        }
        writeEntry(entry, hash, score, depth, bound, move, generation_);
        return;
    }

    for (TTEntry& entry : cluster.entries) {
        if (entry.hash == 0) {
            writeEntry(entry, hash, score, depth, bound, move, generation_);
            return;
        }
    }

    TTEntry* victim = &cluster.entries[0];
    for (TTEntry& entry : cluster.entries) {
        if (replacementPriority(entry, generation_) < replacementPriority(*victim, generation_))
            victim = &entry;
    }

    if (replacementPriority(depth, bound) < replacementPriority(*victim, generation_))
        return;

    writeEntry(*victim, hash, score, depth, bound, move, generation_);
}

int TranspositionTable::hashFull() const {
    if (clusters_.empty()) return 0;
    int used = 0;
    int sampled = 0;
    for (const TTCluster& cluster : clusters_) {
        for (const TTEntry& entry : cluster.entries) {
            if (sampled >= 1000)
                break;
            if (entry.hash != 0)
                used++;
            sampled++;
        }
        if (sampled >= 1000)
            break;
    }
    return sampled == 0 ? 0 : static_cast<int>(used * 1000 / sampled);
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
