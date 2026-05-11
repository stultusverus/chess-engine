# Progress

## Phase 1 — Chess Engine Core ✅

- [x] `src/engine/types.h/cpp` — Core types: Square, Piece, Color, Move, Bitboard, CastlingRights
- [x] `src/engine/attacks.h/cpp` — Magic bitboard attack tables (bishop/rook/queen), pawn/knight/king precomputed attacks
- [x] `src/engine/board.h/cpp` — Bitboard representation, make/unmake move, FEN import/export, Zobrist hashing
- [x] `src/engine/movegen.h/cpp` — Magic bitboard move generation, legal move filtering. Perft-verified
- [x] `src/engine/eval.h/cpp` — Tapered evaluation with PeSTO piece-square tables (MG/EG)
- [x] `src/engine/search.h/cpp` — Alpha-beta with PVS, iterative deepening, quiescence, LMR, null move pruning (zugzwang guard), killer/history heuristic
- [x] `src/engine/tt.h/cpp` — Transposition table (always-replace, 16B entries)
- [x] `src/engine/book.h/cpp` — Polyglot opening book loader (.bin format, weighted random selection)
- [x] `src/engine/uci.h/cpp` — UCI protocol (position, go, stop, setoption, time management, WDL support)
- [x] `src/engine/poly_keys.h` — Polyglot Zobrist key constants (header-only)

## Phase 2 — Testing & Polish

- [x] `CMakeLists.txt` — Build system with auto-fetched dependencies
- [x] `test/test_board.cpp` — Board unit tests
- [x] `test/test_movegen.cpp` — Move generation tests. Perft suite
- [x] `test/test_eval.cpp` — Evaluation tests (material, piece-square, game phase, constants)
- [x] `test/test_search.cpp` — Search tests (mate detection, captures, promotions, TT, nodes, stop)
- [x] `test/test_book.cpp` — Opening book tests (hash correctness, load/probe, decode, maxPly, weighted random)
- [x] Evaluation improvements — king safety, pawn structure, mobility, bishop pair, rook files, tempo
- [x] Engine state and UCI stop handling fixed — correct board rebuild on `go`, thread-safe stop
- [x] Promotion move serialization fix — underpromotions now correctly reported in UCI output
- [x] UCI WDL support — `UCI_ShowWDL` option for Win/Draw/Loss probability output
- [x] SPRT regression testing — fastchess bench with frozen baseline (`bench/run-fastchess-sprt.sh`)
- [x] Lichess bot deployment script — `scripts/setup-lichess-bot-runtime.sh` (uses official lichess-bot client)
