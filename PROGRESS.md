# Progress

## Phase 1 — Chess Engine Core ✅

- [x] `src/engine/types.h` — Core types: Square, Piece, Color, Move, Bitboard, CastlingRights
- [x] `src/engine/attacks.h/cpp` — Magic bitboard attack tables (bishop/rook/queen), pawn/knight/king precomputed attacks
- [x] `src/engine/board.h/cpp` — Bitboard representation, make/unmake move, FEN import/export, Zobrist hashing
- [x] `src/engine/movegen.h/cpp` — Magic bitboard move generation, legal move filtering. Perft-verified
- [x] `src/engine/eval.h/cpp` — Tapered evaluation with PeSTO piece-square tables (MG/EG)
- [x] `src/engine/search.h/cpp` — Alpha-beta with PVS, iterative deepening, quiescence, LMR, killer/history
- [x] `src/engine/tt.h/cpp` — Transposition table (always-replace, 16B entries)
- [x] `src/engine/uci.h/cpp` — UCI protocol (position, go, stop, setoption, time management)

## Phase 2 — Lichess Bot Client

- [x] `src/bot/client.h/cpp` — HTTP client (libcurl), all Bot API endpoints (challenge, move, chat, abort, resign, draw)
- [x] `src/bot/manager.h/cpp` — Global event loop, challenge filtering, concurrent game orchestration

## Phase 3 — Full Bot Features

- [ ] `src/config.h` — CLI argument parsing, config loading
- [ ] `src/main.cpp` — Entry point, signal handling, graceful shutdown
- [ ] Auto-resign when eval drops below threshold for N moves
- [ ] Auto-accept/offer draw when eval is near zero
- [ ] Chat messages (GG on game end)
- [ ] CMakeLists.txt — Build system
- [ ] Tests — Unit tests for board, movegen, eval, search
- [ ] Integration test — Play a full game against the engine locally via UCI
