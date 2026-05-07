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

## Phase 2 — Lichess Bot Client ✅

- [x] `src/bot/client.h/cpp` — HTTP client (libcurl), NDJSON streaming, all Bot API endpoints (challenge, move, chat, abort, resign, draw)
- [x] `src/bot/manager.h/cpp` — Global event loop, challenge filtering, concurrent game orchestration

## Phase 3 — Full Bot Features

- [x] `CMakeLists.txt` — Build system with auto-fetched dependencies
- [x] `src/main.cpp` + `src/bot_main.cpp` — UCI and bot entry points
- [x] `test/test_board.cpp` + `test/test_movegen.cpp` — Board + movegen unit tests. Perft suite
- [x] `test/test_eval.cpp` — Evaluation tests (material, piece-square, game phase, constants)
- [x] `test/test_search.cpp` — Search tests (mate detection, captures, promotions, TT, nodes, stop)
- [x] End-to-end verified — bot plays complete games on Lichess vs Stockfish AI
- [ ] Auto-resign when eval drops below threshold
- [ ] Auto-accept/offer draw when eval is near zero
- [ ] Chat messages (GG on game end)
- [ ] Move overrides (resign/draw commands from operator)
- [ ] Opening book support
- [ ] Search evaluation improvements (king safety, pawn structure, mobility)
