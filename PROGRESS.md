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

Verification:
- [x] `cmake --build build -j 8`
- [x] `ctest --test-dir build --output-on-failure` — 5/5 tests passed in 3.29s (was 49.69s)

Problems found and fixed:
- [x] Move validation boundary is unsafe. `Board::makeMove` only checks source occupancy/color and final king safety, then treats any occupied destination as a capture; it does not reject friendly-piece captures or non-pseudo-legal piece movement before mutating the board (`src/engine/board.cpp:189`, `src/engine/board.cpp:203`, `src/engine/board.cpp:271`). `UCI::handlePosition` also ignores the return value from `makeMove` (`src/engine/uci.cpp:159`), so malformed move tokens can silently corrupt or desynchronize the engine position.
  - **Fixed**: Added friendly-piece capture check before board mutation. Added pseudo-legal piece movement verification (pawn push/capture, knight/bishop/rook/queen/king attack tables) that rejects moves like `e2e5` or `b1b3`. `handlePosition` now checks `makeMove` return value, logs illegal moves to stderr, and stops processing moves.
- [x] Malformed UCI move squares unchecked. `stringToSquare()` (`src/engine/types.cpp:13`) converts strings to squares without file/rank validation, and `src/engine/uci.cpp:163` passes those squares into `makeMove`. Invalid squares like `i2i4` and `a9a8` could drive out-of-bounds mailbox access.
  - **Fixed**: Added file 'a'-'h' and rank '1'-'8' character range checks in `stringToSquare()`, returning `SQ_NONE` on invalid input.
- [x] FEN placement parser bounds. The placement parser in `src/engine/board.cpp:88` advances `sq` without bounds checks and can write `mailbox_[sq]` out of bounds for malformed placement fields.
  - **Fixed**: Added bounds checks after each `sq` advance; rejects digits outside 1-8 range; validates rank transition on '/' is non-negative; bails out on overflow/underflow.
- [x] Polyglot castling moves are decoded as raw Polyglot from/to squares and are not converted to UCI castling destinations (`src/engine/book.cpp:60`). Polyglot encodes castling as king-to-rook (`e1h1`, `e1a1`, `e8h8`, `e8a8`), but UCI needs `e1g1`, `e1c1`, `e8g8`, `e8c8`; a book castling hit can therefore emit an invalid `bestmove`.
  - **Fixed**: Added `fixupPolyglotMove()` in `book.cpp` that remaps the four castling patterns from Polyglot king-to-rook to UCI king destination.
- [x] The transposition table is disabled by default despite the UCI option advertising `Hash` default 64 MB (`src/engine/uci.cpp:118`). `Search::Search` never sizes the table (`src/engine/search.cpp:13`), and `TranspositionTable::probe/store` are no-ops when `entries_` is empty (`src/engine/tt.cpp:24`, `src/engine/tt.cpp:35`).
  - **Fixed**: `Search::Search()` now calls `tt_.setSize(64)` to allocate the advertised default.
- [x] TT score storage truncates mate scores. `TTEntry::score` is `int16_t` (`src/engine/tt.h:17`), but search stores scores near +/-1,000,000 for mates via `static_cast<int16_t>` (`src/engine/search.cpp:243`). Once Hash is enabled, mate bounds/exact scores can be read back as unrelated centipawn values.
  - **Fixed**: Changed `TTEntry::score` from `int16_t` to `int32_t`. Removed the now-unnecessary `uint16_t padding` field (entry stays 16 bytes). Updated `store()` signature and call sites.
- [x] Root TT exact hits can return a score without restoring a root move. `bestMoveRoot_` is reset at the start of every search (`src/engine/search.cpp:37`), but an exact TT return exits before root move selection (`src/engine/search.cpp:110`, `src/engine/search.cpp:118`), leaving callers to fall back to the first legal move instead of the stored best move.
  - **Fixed**: In `alphaBeta()`, TT early-return sites now set `bestMoveRoot_ = ttMove` for EXACT and LOWER bound returns when `ply == 0`.
- [x] Startup and tests are dominated by runtime magic-number search. `attacks::init` searches bishop/rook magics on every process startup (`src/engine/attacks.cpp:127`, `src/engine/attacks.cpp:165`, `src/engine/attacks.cpp:239`); direct startup measured about 8.5s, and the current CTest suite spends about 50s mostly repeating this work. The failure path also silently sets `magic = 0` (`src/engine/attacks.cpp:144`, `src/engine/attacks.cpp:182`) instead of failing loudly.
  - **Fixed**: Embedded 128 known-good magic numbers as `constexpr uint64_t[64]` arrays. Replaced runtime brute-force search with direct assignment and fast table-generation loops. Startup now ~0.3s, test suite 3.29s.
- [x] Iterative search info is written to an `engine.err` file instead of UCI stdout or debug stderr (`src/engine/search.cpp:78`). This creates an unexpected runtime file and prevents GUIs from receiving normal depth-by-depth `info` updates; `UCI::emitSearchInfo` only prints once after search completion (`src/engine/uci.cpp:68`).
  - **Fixed**: Replaced `std::ofstream("engine.err")` file write with `std::cout << "info ..."` to stdout per UCI protocol spec.
- [x] FEN parsing is not hardened and can throw or read invalid tokens on malformed input. `Board::setFen` uses `std::stoi` and unchecked token indexing (`src/engine/board.cpp:116`), which conflicts with the repository's no-exceptions convention and can crash the UCI process on bad `position fen` input.
  - **Fixed**: Replaced `std::stoi` with a `safeParseInt` helper using `std::strtol` with error checking. Added bounds validation for en passant file/rank characters. Added bounds checks in FEN placement parser for square underflow/overflow.
