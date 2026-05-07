#include "board.h"
#include "attacks.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <cctype>

namespace chess {

// --- Zobrist keys ---
namespace {
    uint64_t zobristPieces_[PIECE_NB][64];
    uint64_t zobristCastle_[16];
    uint64_t zobristEp_[8];
    uint64_t zobristSide_;

    uint64_t prng(uint64_t& state) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }

    // Castling rights on each square (cleared when square is vacated)
    constexpr int castleRightsMask[64] = {
        WQ, 0, 0, 0, WK|WQ, 0, 0, WK,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        BQ, 0, 0, 0, BK|BQ,  0, 0, BK,
    };
}

void Board::initZobrist() {
    uint64_t state = 0x123456789ABCDEF0ULL;
    for (int p = 0; p < PIECE_NB; p++)
        for (int s = 0; s < 64; s++)
            zobristPieces_[p][s] = prng(state);
    zobristSide_ = prng(state);
    for (int i = 0; i < 16; i++)
        zobristCastle_[i] = prng(state);
    for (int f = 0; f < 8; f++)
        zobristEp_[f] = prng(state);
}

// --- Board ---

Board::Board() {
    setFen(STARTPOS_FEN);
}

Board::Board(const std::string& fen) {
    setFen(fen);
}

void Board::setFen(const std::string& fen) {
    std::fill(byPiece_.begin(), byPiece_.end(), Bitboard(0));
    std::fill(byColor_.begin(), byColor_.end(), Bitboard(0));
    std::fill(mailbox_.begin(), mailbox_.end(), NO_PIECE);

    std::istringstream ss(fen);
    std::string placement, stm, castle, ep, half, full;

    ss >> placement >> stm >> castle >> ep >> half >> full;

    // Piece placement
    Square sq = A8;
    for (char c : placement) {
        if (c == '/') {
            int prevRank = rankOf(Square(sq - 1));
            sq = makeSquare(FILE_A, Rank(prevRank - 1));
        } else if (std::isdigit(c)) {
            int empty = c - '0';
            for (int i = 0; i < empty; i++)
                sq = Square(sq + 1);
        } else {
            Piece p = NO_PIECE;
            switch (c) {
            case 'P': p = W_PAWN; break; case 'N': p = W_KNIGHT; break;
            case 'B': p = W_BISHOP; break; case 'R': p = W_ROOK; break;
            case 'Q': p = W_QUEEN; break; case 'K': p = W_KING; break;
            case 'p': p = B_PAWN; break; case 'n': p = B_KNIGHT; break;
            case 'b': p = B_BISHOP; break; case 'r': p = B_ROOK; break;
            case 'q': p = B_QUEEN; break; case 'k': p = B_KING; break;
            }
            if (p != NO_PIECE) {
                byPiece_[p] |= squareBb(sq);
                byColor_[colorOf(p)] |= squareBb(sq);
                mailbox_[sq] = p;
            }
            sq = Square(sq + 1);
        }
    }

    // Side to move
    stm_ = (stm == "b") ? BLACK : WHITE;

    // Castling rights
    if (castle.find('K') != std::string::npos) castle_ |= WK;
    if (castle.find('Q') != std::string::npos) castle_ |= WQ;
    if (castle.find('k') != std::string::npos) castle_ |= BK;
    if (castle.find('q') != std::string::npos) castle_ |= BQ;

    // En passant
    if (ep != "-") {
        File f = File(ep[0] - 'a');
        Rank r = Rank(ep[1] - '1');
        ep_ = makeSquare(f, r);
    }

    // Clocks
    halfMoves_ = half.empty() ? 0 : std::stoi(half);
    fullMoves_ = full.empty() ? 1 : std::stoi(full);

    // Build hash
    hash_ = 0;
    for (int s = 0; s < 64; s++) {
        Piece p = mailbox_[s];
        if (p != NO_PIECE) hash_ ^= zobristPieces_[p][s];
    }
    if (stm_ == BLACK) hash_ ^= zobristSide_;
    hash_ ^= zobristCastle_[castle_];
    if (ep_ != SQ_NONE) hash_ ^= zobristEp_[fileOf(ep_)];
}

std::string Board::fen() const {
    std::ostringstream ss;
    for (Rank r = RANK_8; r >= RANK_1; r = Rank(r - 1)) {
        int empty = 0;
        for (File f = FILE_A; f <= FILE_H; f = File(f + 1)) {
            Square s = makeSquare(f, r);
            Piece p = mailbox_[s];
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty) { ss << empty; empty = 0; }
                const char* pc = "PNBRQKpnbrqk";
                ss << pc[p];
            }
        }
        if (empty) ss << empty;
        if (r > RANK_1) ss << '/';
    }

    ss << ' ' << (stm_ == WHITE ? 'w' : 'b');
    ss << ' ';
    if (castle_ == 0) ss << '-';
    else {
        if (castle_ & WK) ss << 'K';
        if (castle_ & WQ) ss << 'Q';
        if (castle_ & BK) ss << 'k';
        if (castle_ & BQ) ss << 'q';
    }
    ss << ' ' << (ep_ == SQ_NONE ? "-" : squareToString(ep_));
    ss << ' ' << halfMoves_ << ' ' << fullMoves_;
    return ss.str();
}

void Board::putPiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] |= squareBb(s);
    byColor_[colorOf(p)] |= squareBb(s);
    mailbox_[s] = p;
    hash_ ^= zobristPieces_[p][s];
}

void Board::removePiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] &= ~squareBb(s);
    byColor_[colorOf(p)] &= ~squareBb(s);
    mailbox_[s] = NO_PIECE;
    hash_ ^= zobristPieces_[p][s];
}

void Board::movePiece(Piece p, Square from, Square to) {
    removePiece(p, from);
    putPiece(p, to);
}

bool Board::makeMove(Move move, UndoInfo& undo) {
    undo.oldEp = ep_;
    undo.oldCastle = castle_;
    undo.oldHalfMoves = halfMoves_;
    undo.oldHash = hash_;
    undo.captured = NO_PIECE;

    Piece p = mailbox_[move.from];
    if (p == NO_PIECE) return false;
    Color us = stm_;
    if (colorOf(p) != us) return false;
    PieceType pt = typeOf(p);

    // Classify move if not pre-classified
    if (move.type == NORMAL) {
        if (pt == PAWN && move.to == ep_)
            move.type = EN_PASSANT;
        else if (pt == PAWN && (rankOf(move.to) == RANK_8 || rankOf(move.to) == RANK_1))
            move.type = (mailbox_[move.to] != NO_PIECE) ? PROMOTION_CAPTURE : PROMOTION;
        else if (pt == KING && (move.to == move.from + 2 || move.to == move.from - 2))
            move.type = CASTLING;
        else if (mailbox_[move.to] != NO_PIECE)
            move.type = CAPTURE;
    }

    undo.move = move;

    // Remove piece from source
    removePiece(p, move.from);

    // Handle captures
    switch (move.type) {
    case CAPTURE:
    case PROMOTION_CAPTURE:
        undo.captured = mailbox_[move.to];
        removePiece(undo.captured, move.to);
        break;
    case EN_PASSANT:
        if (us == WHITE) {
            Square cs = Square(move.to - 8);
            undo.captured = mailbox_[cs];
            removePiece(undo.captured, cs);
        } else {
            Square cs = Square(move.to + 8);
            undo.captured = mailbox_[cs];
            removePiece(undo.captured, cs);
        }
        break;
    default:
        break;
    }

    // Place piece on destination
    Piece destPiece = p;
    if (move.type == PROMOTION || move.type == PROMOTION_CAPTURE)
        destPiece = makePiece(us, move.promotion);
    putPiece(destPiece, move.to);

    // Handle castling (rook move)
    if (move.type == CASTLING) {
        if (move.to == G1)      movePiece(W_ROOK, H1, F1);
        else if (move.to == C1) movePiece(W_ROOK, A1, D1);
        else if (move.to == G8) movePiece(B_ROOK, H8, F8);
        else if (move.to == C8) movePiece(B_ROOK, A8, D8);
    }

    // Update castling rights
    int oldCastle = castle_;
    castle_ &= ~castleRightsMask[move.from];
    castle_ &= ~castleRightsMask[move.to];
    if (oldCastle != castle_) {
        hash_ ^= zobristCastle_[oldCastle];
        hash_ ^= zobristCastle_[castle_];
    }

    // Update en passant
    Square oldEp = ep_;
    ep_ = SQ_NONE;
    if (pt == PAWN && ((us == WHITE && rankOf(move.to) == RANK_4 && rankOf(move.from) == RANK_2) ||
                       (us == BLACK && rankOf(move.to) == RANK_5 && rankOf(move.from) == RANK_7))) {
        ep_ = Square(us == WHITE ? move.from + 8 : move.from - 8);
    }
    if (oldEp != ep_) {
        if (oldEp != SQ_NONE) hash_ ^= zobristEp_[fileOf(oldEp)];
        if (ep_ != SQ_NONE)   hash_ ^= zobristEp_[fileOf(ep_)];
    }

    // Update half-move clock
    if (pt == PAWN || move.type == CAPTURE || move.type == EN_PASSANT)
        halfMoves_ = 0;
    else
        halfMoves_++;

    // Update full move number
    if (us == BLACK) fullMoves_++;

    // Toggle side
    hash_ ^= zobristSide_;
    stm_ = ~us;

    // Check legality: after the move, is the moving side's king in check?
    // The side that just moved is `us`; opponent is now `stm_`
    Square kSq = kingSquare(us);
    if (attacks::isSquareAttacked(*this, kSq, stm_)) {
        unmakeMove(move, undo);
        return false;
    }

    return true;
}

void Board::unmakeMove(Move /*move*/, const UndoInfo& undo) {
    const Move& m = undo.move;
    Piece p = mailbox_[m.to];
    Color us = ~stm_; // The side that made the move
    PieceType pt = typeOf(p);

    // Undo side toggle
    hash_ = undo.oldHash;

    // Restore state
    stm_ = us;
    ep_ = undo.oldEp;
    castle_ = undo.oldCastle;
    halfMoves_ = undo.oldHalfMoves;

    // Remove piece from destination
    if (m.type == PROMOTION || m.type == PROMOTION_CAPTURE) {
        removePiece(p, m.to);
        putPiece(makePiece(us, PAWN), m.from);
    } else {
        removePiece(p, m.to);
        putPiece(p, m.from);
    }

    // Restore captured pieces
    switch (m.type) {
    case CAPTURE:
    case PROMOTION_CAPTURE:
        putPiece(undo.captured, m.to);
        break;
    case EN_PASSANT:
        if (us == WHITE)
            putPiece(undo.captured, Square(m.to - 8));
        else
            putPiece(undo.captured, Square(m.to + 8));
        break;
    default:
        break;
    }

    // Undo castling rook move
    if (m.type == CASTLING) {
        if (m.to == G1)      movePiece(W_ROOK, F1, H1);
        else if (m.to == C1) movePiece(W_ROOK, D1, A1);
        else if (m.to == G8) movePiece(B_ROOK, F8, H8);
        else if (m.to == C8) movePiece(B_ROOK, D8, A8);
    }
}

void Board::makeNullMove(NullUndo& undo) {
    undo.oldEp = ep_;
    undo.oldHalfMoves = halfMoves_;
    undo.oldHash = hash_;

    hash_ ^= zobristSide_;
    stm_ = ~stm_;

    if (ep_ != SQ_NONE) {
        hash_ ^= zobristEp_[fileOf(ep_)];
        ep_ = SQ_NONE;
    }

    halfMoves_++;
}

void Board::unmakeNullMove(const NullUndo& undo) {
    stm_ = ~stm_;
    ep_ = undo.oldEp;
    halfMoves_ = undo.oldHalfMoves;
    hash_ = undo.oldHash;
}

bool Board::isMoveLegal(Move move) const {
    Board temp = *this;
    UndoInfo undo;
    return temp.makeMove(move, undo);
}

bool Board::isInCheck() const {
    return attacks::isSquareAttacked(*this, kingSquare(stm_), ~stm_);
}

Piece Board::pieceOn(Square s) const { return mailbox_[s]; }

Bitboard Board::pieces() const { return byColor_[WHITE] | byColor_[BLACK]; }
Bitboard Board::pieces(Color c) const { return byColor_[c]; }
Bitboard Board::pieces(PieceType pt) const { return byPiece_[makePiece(WHITE, pt)] | byPiece_[makePiece(BLACK, pt)]; }
Bitboard Board::pieces(Color c, PieceType pt) const { return byPiece_[makePiece(c, pt)]; }
Bitboard Board::occupied() const { return pieces(); }
Bitboard Board::empty() const { return ~pieces(); }

Square Board::kingSquare(Color c) const { return lsb(pieces(c, KING)); }

} // namespace chess
