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

## Repository Review — 2026-05-11

Verification performed:
- `cmake --build build --parallel 2` — passed
- `ctest --test-dir build --output-on-failure` — 6/6 tests passed
- Targeted probes for generated move metadata, UCI search output, malformed FEN behavior, and the SPRT baseline binary

Open problems found:

- [ ] `MoveGenerator` emits legal moves with `Move::type == NORMAL` even for captures, promotions, en passant, and castling. `Board::isMoveLegal()` classifies only a by-value copy, so the stored move list keeps stale metadata. Search then treats tactical moves as quiet moves for ordering, futility pruning, LMR, killer/history updates, and quiescence; in practice quiescence filters out captures/promotions because their type is still `NORMAL`. Example probe: a generated queen capture `d4d5` reported `type=0`.
- [ ] Quiescence search allows stand-pat while the side to move is in check. `alphaBeta()` enters `quiesce()` when `depth <= 0` before applying any in-check extension, and `quiesce()` evaluates/cuts on `standPat` before checking `board.isInCheck()`. This can score a leaf check as if passing were legal.
- [ ] UCI output is duplicated and coupled to the core search. `Search::search()` prints `info` lines directly to `stdout`, then `UCI::handleGo()` emits another final `info` line for the same completed search. Mate scores are also printed as huge centipawn values such as `score cp 999899` instead of UCI `score mate N`.
- [ ] FEN loading accepts malformed or illegal positions without reporting failure. Positions with missing kings can reach `Board::kingSquare()`, which calls `lsb(0)`; that is undefined behavior and can feed invalid squares into move generation/evaluation. UCI currently accepts an empty-board FEN and searches it instead of rejecting it.
- [ ] `MoveGenerator::generateMoves()` appends to the caller-provided `MoveList` without clearing it first. Reusing a `MoveList` across calls can leave stale moves mixed into the new position's move list.
- [ ] Search does not account for 50-move-rule or repetition draws. `Board` stores the halfmove clock, but `Search` never returns a draw for `halfMoveClock() >= 100` and has no position-history tracking for repetition.
- [ ] The default frozen SPRT baseline is not runnable on this macOS workspace: `bench/chess-engine-old` is a Linux ELF binary and fails with `exec format error`. `bench/run-fastchess-sprt.sh` therefore needs a platform-local `OLD_ENGINE` override or a portable baseline strategy.
- [ ] `CMakeLists.txt` declares, links, and may auto-fetch `nlohmann/json`, but no source file includes or uses it. This adds an unnecessary dependency and can introduce a network requirement in a clean environment.
- [ ] `scripts/setup-lichess-bot-runtime.sh` generates a config with `ponder: true`, but the UCI engine does not advertise a `Ponder` option or handle `ponderhit`. The script also copies `Titans.bin` while leaving both lichess-bot Polyglot and the engine's `OwnBook` option disabled, so the copied book is unused by default.
