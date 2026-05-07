# Progress

## Phase 1 — Chess Engine Core ✅

- [x] `src/engine/types.h` — Core types: Square, Piece, Color, Move, Bitboard, CastlingRights
- [x] `src/engine/attacks.h/cpp` — Magic bitboard attack tables (bishop/rook/queen), pawn/knight/king precomputed attacks
- [x] `src/engine/board.h/cpp` — Bitboard representation, make/unmake move, FEN import/export, Zobrist hashing
- [x] `src/engine/movegen.h/cpp` — Magic bitboard move generation, legal move filtering. Perft-verified
- [x] `src/engine/eval.h/cpp` — Tapered evaluation with PeSTO piece-square tables (MG/EG)
- [x] `src/engine/search.h/cpp` — Alpha-beta with PVS, iterative deepening, quiescence, LMR, killer/history
- [x] `src/engine/tt.h/cpp` — Transposition table (always-replace, 16B entries)
- [x] `src/engine/book.h/cpp` — Polyglot opening book loader (.bin format, weighted random selection)
- [x] `src/engine/uci.h/cpp` — UCI protocol (position, go, stop, setoption, time management)

## Phase 2 — Lichess Bot Client ✅

- [x] `src/bot/client.h/cpp` — HTTP client (libcurl), NDJSON streaming, all Bot API endpoints (challenge, move, chat, abort, resign, draw)
- [x] `src/bot/manager.h/cpp` — Global event loop, challenge filtering, concurrent game orchestration
- [x] `src/bot/decision.h` — Pure decision logic for resign/draw (testable without HTTP/curl)

## Phase 3 — Full Bot Features

- [x] `CMakeLists.txt` — Build system with auto-fetched dependencies
- [x] `src/main.cpp` + `src/bot_main.cpp` — UCI and bot entry points
- [x] `test/test_board.cpp` + `test/test_movegen.cpp` — Board + movegen unit tests. Perft suite
- [x] `test/test_eval.cpp` — Evaluation tests (material, piece-square, game phase, constants)
- [x] `test/test_search.cpp` — Search tests (mate detection, captures, promotions, TT, nodes, stop)
- [x] `test/test_book.cpp` — Opening book tests (hash correctness, load/probe, decode, maxPly, weighted random)
- [x] `test/test_bot.cpp` — Bot decision tests (resign, draw accept, draw offer, threshold logic)
- [x] End-to-end verified — bot plays complete games on Lichess vs Stockfish AI
- [x] Auto-resign when eval drops below threshold
- [x] Auto-accept/offer draw when eval is near zero
- [ ] Chat messages (GG on game end)
- [ ] Move overrides (resign/draw commands from operator)
- [x] Opening book support — Polyglot (.bin) format, `src/engine/book.h/cpp`, `--book` flag
- [x] Book polish — fixed RNG seeding, bswap32, bulk-read, deduplicated probe logic, unit tests
- [x] UCI position parser fix — moves after `position startpos` now correctly applied
- [ ] Search evaluation improvements (king safety, pawn structure, mobility)
