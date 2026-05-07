#pragma once

#include "types.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chess {

class Board;

class Book {
public:
    Book() = default;
    ~Book() = default;

    bool load(const std::string& path);
    bool isLoaded() const { return !entries_.empty(); }

    void setMaxPly(int ply) { maxHalfMoves_ = ply * 2; }
    int maxPly() const { return maxHalfMoves_ / 2; }

    std::optional<Move> probe(const Board& board) const;
    static uint64_t polyglotHash(const Board& board);
    static Move decodePolyglotMove(uint16_t packed);

private:
    struct Entry {
        uint64_t key;
        uint16_t move;
        uint16_t weight;
        uint32_t learn;
    };

    std::vector<Entry> entries_;
    int maxHalfMoves_ = 20;
};

} // namespace chess
