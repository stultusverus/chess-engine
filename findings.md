# Repository Analysis Findings

Date: 2026-05-12

## Resolved Findings

## 1) UCI mate score regression was timing-sensitive in local/cloud runs

- **Status:** Resolved in `f9b78c4` (`Stabilize UCI mate score test`)
- **Previous severity:** High
- **Previous evidence:** `test_uci` could fail in `test_mateScoresUseUciMateFormat` when the async search was stopped before completing the intended depth.
- **Resolution:** The regression now searches a mate-in-one at depth 1 and gives the engine enough time to complete before `quit`, keeping the assertion focused on UCI `score mate N` formatting rather than host speed.
- **Relevant code:** `test/test_uci.cpp`

## 2) UCI `position fen` parser did not validate token availability before building FEN

- **Status:** Resolved in `1e3db28` (`Harden UCI position parsing`)
- **Previous severity:** Medium
- **Previous evidence:** `UCI::handlePosition` performed 6 extractions for FEN fields without checking stream state.
- **Resolution:** Incomplete FEN commands now emit `[uci] illegal fen: <partial-fen>` and return before replacing the current board.
- **Regression coverage:** `test_incompleteFenDoesNotReplaceCurrentPosition`
- **Relevant code:** `src/engine/uci.cpp`, `test/test_uci.cpp`

## 3) `position` command accepted unknown position type silently

- **Status:** Resolved in `1e3db28` (`Harden UCI position parsing`)
- **Previous severity:** Low
- **Previous evidence:** Unknown position types could fall through into move parsing against the previous board.
- **Resolution:** Unknown position types now emit `[uci] illegal position: <type>` and return without parsing moves or mutating the board.
- **Regression coverage:** `test_unknownPositionTypeDoesNotMutateBoard`
- **Relevant code:** `src/engine/uci.cpp`, `test/test_uci.cpp`

## Current Test Status

- Build succeeds (`cmake`, `make`).
- 6/6 tests pass (`board`, `movegen`, `eval`, `search`, `book`, `uci`).

## Verification

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j2
ctest --output-on-failure
```

## Missing Features Analysis

Compared with the repository goals (competitive UCI engine + robust deployment/regression workflow), the following high-value features are still missing or incomplete:

1. **UCI protocol completeness for bot/GUI interoperability**
   - Missing `Ponder` option and `ponderhit` command handling.
   - Diagnostics for malformed `position` input are improved, but broader unsupported command diagnostics remain limited.
   - No richer principal variation output (single-move PV only) and no multi-PV mode.

2. **Search robustness and tactical correctness safeguards**
   - No dedicated draw adjudication workflow exposed/verified for practical threefold and 50-move edge cases under long search sessions.
   - No explicit aspiration-window recovery instrumentation/telemetry for tuning.
   - No clear benchmark harness for node-speed and tactical suite progression over time.

3. **Evaluation/search feature depth expected in stronger engines**
   - No NNUE/modern learned evaluation backend.
   - No endgame tablebase probing (e.g., Syzygy) for exact play in low-piece endgames.
   - No contempt/draw-bias tuning controls for match strategy.

4. **Developer workflow and QA maturity**
   - No CI pipeline in-repo for automatic build/test regression gates.
   - No sanitizers/fuzzing jobs for parser and move-make/unmake invariants.
   - Dependency and portability hygiene issues remain (unused json dependency, platform-dependent baseline binary behavior).

## Proposed Next Steps

### Immediate (next 1–2 iterations)

1. **Stabilize testing and reproducibility**
   - Add a CI workflow running `cmake`, `make`, and `ctest --output-on-failure` on Linux.
   - Add sanitizer build variants (ASan/UBSan) and run unit tests under them.

2. **Clean dependency/runtime friction**
   - Remove or gate `nlohmann/json` fetch if unused.
   - Update SPRT script/docs to require/auto-detect a platform-compatible baseline engine.

3. **Extend UCI regression coverage**
   - Add a losing-mate `score mate -N` regression test.
   - Add tests for malformed `go` numeric arguments and unsupported command variants if stricter diagnostics are desired.

### Near-term (next 3–6 iterations)

4. **Complete UCI interoperability**
   - Implement `option name Ponder`, `go ponder`, and `ponderhit` flows.
   - Emit fuller PV lines and add optional `MultiPV` support for analysis GUIs.

5. **Improve search quality with measurable gates**
   - Add tactical test corpus runner (e.g., EPD/WAC-style checks) and track solved positions at fixed depth/time.
   - Add lightweight profiling hooks for node distribution, qsearch share, and prune hit rates.

6. **Deployment quality upgrades**
   - Align lichess setup defaults with actual engine capabilities (disable ponder by default unless implemented; document opening-book behavior clearly).

### Strategic (medium horizon)

7. **Strength roadmap**
   - Introduce optional Syzygy probing.
   - Evaluate a staged NNUE integration plan behind a compile/runtime option.
   - Use SPRT to validate every strength feature behind reproducible parameter sets.
