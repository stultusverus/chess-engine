# Progress

## Phase 1 — Chess Engine Core

- [x] `src/engine/types.h` — Core types: Square, Piece, Color, Move, Bitboard, CastlingRights
- [x] `src/engine/attacks.h/cpp` — Magic bitboard attack tables (bishop/rook/queen), pawn/knight/king precomputed attacks
- [x] `src/engine/board.h/cpp` — Bitboard representation, make/unmake move, FEN import/export, Zobrist hashing
- [ ] `src/engine/movegen.h/cpp` — Magic bitboard move generation, legal move filtering, move ordering (MVV-LVA)
- [ ] `src/engine/eval.h/cpp` — Material counting, piece-square tables, pawn structure evaluation
- [ ] `src/engine/search.h/cpp` — Negamax alpha-beta, iterative deepening, quiescence search, time management
- [ ] `src/engine/tt.h/cpp` — Transposition table with Zobrist keys, always-replace scheme
- [ ] `src/engine/uci.h/cpp` — UCI protocol parser, `position`, `go`, `stop`, `setoption` commands

## Phase 2 — Lichess Bot Client

- [ ] `src/bot/client.h/cpp` — HTTP client, all Bot API endpoints (move, challenge, abort, resign, draw, chat)
- [ ] `src/bot/stream.h/cpp` — NDJSON stream parser, event type dispatch (gameStart, gameFinish, challenge, gameState, chatLine)
- [ ] `src/bot/manager.h/cpp` — Global event loop, challenge filtering, concurrent game orchestration

## Phase 3 — Full Bot Features

- [ ] `src/config.h` — CLI argument parsing, config loading
- [ ] `src/main.cpp` — Entry point, signal handling, graceful shutdown
- [ ] Auto-resign when eval drops below threshold for N moves
- [ ] Auto-accept/offer draw when eval is near zero
- [ ] Chat messages (GG on game end)
- [ ] CMakeLists.txt — Build system
- [ ] Tests — Unit tests for board, movegen, eval, search
- [ ] Integration test — Play a full game against the engine locally via UCI
