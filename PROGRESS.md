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

- [x] `CMakeLists.txt` — Build system with engine, tests, and benchmark target
- [x] `test/test_board.cpp` — Board unit tests
- [x] `test/test_movegen.cpp` — Move generation tests. Perft suite
- [x] `test/test_eval.cpp` — Evaluation tests (material, piece-square, game phase, constants)
- [x] `test/test_search.cpp` — Search tests (mate detection, captures, promotions, TT, nodes, stop)
- [x] `test/test_book.cpp` — Opening book tests (hash correctness, load/probe, decode, maxPly, weighted random)
- [x] Evaluation improvements — king safety, pawn structure, mobility, bishop pair, rook files, tempo
- [x] Engine state and UCI stop handling fixed — correct board rebuild on `go`, thread-safe stop
- [x] Promotion move serialization fix — underpromotions now correctly reported in UCI output
- [x] UCI WDL support — `UCI_ShowWDL` option for Win/Draw/Loss probability output
- [x] SPRT regression testing — fastchess script with versioned binaries (`scripts/run-fastchess-sprt.sh`)
- [x] Lichess bot deployment script — `scripts/setup-lichess-bot-runtime.sh` (uses official lichess-bot client)

## Phase 3 — Protocol, QA, and Performance

- [x] Strict FEN validation — rejects malformed clocks, castling fields, impossible en-passant squares, adjacent kings, and invalid fullmove counters without mutating previous board state.
- [x] Hardened UCI `go` parsing — validates numeric values, supports `nodes`, `searchmoves`, `mate`, `infinite`, unsupported `ponder` diagnostics, and `movetime 0`.
- [x] Search reporting — per-depth UCI info callbacks include score, WDL when enabled, nodes, time, nps, hashfull, and PV.
- [x] Move generation/search path — search uses mutable make/unmake legality filtering instead of copying the board for each pseudo-legal move.
- [x] MultiPV — UCI option supports up to four serial root lines for analysis GUIs.
- [x] Benchmark target — `bench_engine` runs fixed perft/search metrics and optional EPD tactical checks.
- [x] CI — GitHub Actions Release and Debug ASan/UBSan builds run `ctest --output-on-failure`.
- [x] Perft recursion uses make/unmake instead of copying boards at every ply.
- [x] Quiescence noisy-move generation — qsearch now uses dedicated legal captures/en-passant/promotions outside check instead of generating all legal moves and filtering.
- [x] Real static exchange evaluation — capture ordering and quiescence pruning use SEE with x-ray, pin, promotion, and en-passant handling.
- [x] Incremental material/PST state, pawn hash, and eval cache — board make/unmake now maintains eval state, pawn structure is cached by pawn hash, and full evals are cached by position hash.

## Current Review Findings

- [x] Fix root TT interaction with `searchmoves` and serial MultiPV. TT hits no longer return a root move outside the active root restriction set.
- [x] Make opening-book probing respect `searchmoves`, analysis mode, and MultiPV, and validate book moves before returning them.
- [x] Normalize en-passant FEN/hash handling. Syntactically valid EP targets are accepted, but EP is hashed only when an EP capture is possible.
- [x] Remove advertised `Ponder` support until true ponder continuation is implemented.
- [x] Add regression tests for the above UCI/book/FEN edge cases.
- [ ] Fix null-move en-passant hash removal. `Board::makeNullMove` currently toggles side before checking whether the old EP key was present.
- [ ] Make UCI `Hash` an allocation cap by avoiding TT round-up above the requested size.
- [ ] Give serial `MultiPV` searches a shared deadline, or replace serial searches with true single-search MultiPV.
- [ ] Restrict or benchmark broad internal iterative deepening before relying on it as a strength improvement.
- [ ] Decide whether unsupported `go ponder` should be ignored, rejected, or implemented as real ponder.

## Performance Backlog

- [x] Replace pseudo-legal plus make/unmake legal generation with check/pin-aware move generation.
- [x] Add incremental material/PST state, pawn hash, and eval cache.
- [x] Improve search infrastructure with depth-preferred TT replacement and soft/hard time management.
- [x] Improve search tuning with better LMR, reverse futility/razoring, continuation history, and capture history.
- [ ] Add a staged move picker: TT move, good captures/promotions, killers/countermoves, quiet history, then bad captures.
- [ ] Avoid eager SEE for every capture during full move sorting; compute SEE lazily where possible.
- [ ] Upgrade TT replacement with 2-way/4-way clustered buckets and generation/age.
- [ ] Store static evaluation in TT entries if it improves pruning/eval reuse.
- [ ] Add machine-readable speed and tactical benchmark modes around fixed positions and EPD suites.
- [ ] Add optional slow perft assertions behind an environment flag.
- [ ] Add randomized make/unmake invariant tests comparing FEN, hash, pawn hash, eval state, and legal-move consistency.

## Strategic Backlog

- [ ] Optional Syzygy probing behind compile/runtime configuration.
- [ ] Automated classical eval tuning using Texel or SPSA before adding many more hand-tuned terms.
- [ ] NNUE evaluation experiment after SPRT and tactical benchmark baselines are stable.
- [ ] SMP search with UCI `Threads` after single-thread search behavior is stable.
