# Code Review Findings

## Confirmed Issues

1. **P1: UCI constructs `board_` before Zobrist keys are initialized.**
   `UCI` owns `Board board_` in `src/engine/uci.h`, so the board is constructed before `UCI::UCI()` calls `Board::initZobrist()` in `src/engine/uci.cpp`. Since `Board::Board()` immediately calls `setFen()`, the initial UCI position gets a hash built from zero-initialized keys. A valid `go` before any `position` or `ucinewgame` can search with inconsistent TT/repetition hashes.

2. **P2: FEN parsing accepts malformed positions as valid.**
   `Board::setFen()` breaks out of malformed placement parsing without recording failure, defaults invalid side-to-move tokens to white, and only validates king count and pawn ranks. UCI accepts invalid FENs such as bad piece characters, invalid side tokens, and negative clocks with only `readyok`, leaving the engine on a different or illegal position than the GUI intended.

3. **P2: Quiescence can score stalemate as static evaluation.**
   `quiesce()` generates legal moves, filters to captures/promotions when not in check, and returns `alpha` when `searchMoves` is empty. For a quiet stalemate at a depth-0 leaf, that returns stand-pat/static evaluation instead of draw score 0. This is reachable from normal `alphaBeta()` leaf calls, not only from a direct depth-0 root search.

4. **P2: Low-clock time management can spend more time than remains.**
   `UCI::handleGo()` caps the search budget to `myTime / movestogo + increment`, then to `myTime / 2`, then raises it to at least 10 ms. With less than 10 ms remaining, the engine can intentionally search longer than the remaining clock before move overhead is applied. The final budget should be clamped to available time minus overhead.

5. **P2: `go infinite` can terminate and emit a move without `stop`.**
   UCI infinite search should continue until `stop` or `quit`, but `handleGo()` still passes `maxDepth = 64` unless a depth is specified. If the search reaches that depth, it can emit `bestmove` even though no `stop` was received.

6. **P2: Search has no ply guard for fixed-size ply arrays.**
   Root depth is capped, but check extensions and quiescence can keep increasing `ply`. Move ordering indexes `killer1_[ply]`, `killer2_[ply]`, and `history_` without a `ply < MAX_PLY` guard. A long checking line or pathological FEN can run past the fixed arrays. Add an entry guard around search/qsearch or skip ply-indexed heuristics beyond the limit.

## Lower-Severity Notes

- **Internal en-passant hash is overly specific.**
  Polyglot hashing correctly includes the en-passant file only when a pawn can capture. The board Zobrist hash includes any `ep_ != SQ_NONE`. The two hash domains do not need to match, but the internal behavior can reduce TT/repetition reuse for legally equivalent positions.

- **Repetition handling is rule-accurate but conservative for search.**
  `isRepetition()` returns true on the third occurrence (`count >= 2`, because history excludes the current position). That is correct for the rule. Some engines score the first repeat as drawish to avoid voluntarily entering repetitions; that is a search policy choice, not a rule bug.

- **TT replacement can overwrite deeper exact entries.**
  The current same-position guard allows a shallower exact entry to replace a deeper exact entry. This is a strength/performance tradeoff rather than a direct correctness bug.

- **`MoveGenerator::perft()` reuses one `UndoInfo` variable across loop iterations.**
  This is technically correct because `makeMove()` overwrites the fields it uses. It is mildly fragile if future undo fields are added and not initialized.

- **`gamePhase()` handles promotions, but the weighting may need tuning.**
  Promoted pieces are counted because phase iterates over current pieces. A promoted queen contributes queen phase weight, which is intentional behavior but may affect tuning.

## Verification

- `cmake --build build --parallel 2` passed.
- `ctest --test-dir build --output-on-failure` passed: 6/6 tests.
- The deeper printed perft probes in `test_movegen` matched their expected values.
- An ASan/UBSan build completed, but the sanitizer CTest run stalled on this macOS environment and was stopped, so it was not used as evidence.
