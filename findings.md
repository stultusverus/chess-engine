# Repository Analysis Findings

Date: 2026-05-12

## 1) Failing test: mate score is not reported in UCI `score mate` format

- **Severity:** High
- **Evidence:** `ctest --output-on-failure` fails `test_uci` in `test_mateScoresUseUciMateFormat` with:
  - `FAIL: contains(output, "score mate ")`
- **Impact:** UCI clients expect mate scores to be emitted as `score mate N`; incorrect formatting can break GUI interpretation and automated tooling.
- **Likely cause:** The search result returned to UCI for a mating line is not crossing the mate threshold check in `emitSearchInfo`, so it is printed as centipawns instead of mate notation.
- **Relevant code:**
  - `src/engine/uci.cpp` (`emitSearchInfo` mate/cp selection)
  - `src/engine/search.cpp` (mate score propagation and TT adjustments)

## 2) UCI `position fen` parser does not validate token availability before building FEN

- **Severity:** Medium
- **Evidence:** In `UCI::handlePosition`, the code always performs 6 extractions for FEN fields without checking stream state. Missing tokens can produce malformed/partial FEN strings before `setFen` is called.
- **Impact:** Malformed command handling is fragile and can produce confusing diagnostics and edge-case behavior.
- **Relevant code:** `src/engine/uci.cpp` (`handlePosition` loop that reads 6 tokens for FEN)

## 3) `position` command accepts unknown position type silently

- **Severity:** Low
- **Evidence:** `UCI::handlePosition` has explicit branches for `startpos` and `fen`, but no `else` error path when `posType` is something else. It then proceeds to parse move tokens anyway.
- **Impact:** Non-compliant or typoed input can mutate game state unexpectedly (moves may be interpreted on the previous board state) instead of being rejected clearly.
- **Relevant code:** `src/engine/uci.cpp` (`handlePosition` control flow)

## What passed

- Build succeeded (`cmake`, `make`).
- 5/6 tests passed (`board`, `movegen`, `eval`, `search`, `book`).

## Reproduction

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure
```

## Missing Features Analysis

Compared with the repository goals (competitive UCI engine + robust deployment/regression workflow), the following high-value features are still missing or incomplete:

1. **UCI protocol completeness for bot/GUI interoperability**
   - Missing `Ponder` option and `ponderhit` command handling.
   - Limited diagnostics for malformed `position` input and unsupported command variants.
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

1. **Fix correctness blockers first**
   - Resolve mate score UCI formatting failure and add a regression test explicitly asserting `score mate N` for both winning and losing mates.
   - Harden `position` parser validation: reject unknown position types and incomplete FEN token sets with deterministic error output.

2. **Stabilize testing and reproducibility**
   - Add a CI workflow running `cmake`, `make`, and `ctest --output-on-failure` on Linux.
   - Add sanitizer build variants (ASan/UBSan) and run unit tests under them.

3. **Clean dependency/runtime friction**
   - Remove or gate `nlohmann/json` fetch if unused.
   - Update SPRT script/docs to require/auto-detect a platform-compatible baseline engine.

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
