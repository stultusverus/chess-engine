# AGENTS.md

Instructions for AI coding agents working on this repository.
This repository is a C++17 chess engine. Treat `main` as protected even if GitHub
settings technically allow direct pushes or merges.

## Hard Rules

- Do not merge pull requests.
- Do not close issues manually unless the user explicitly asks.
- Do not force-push `main`.
- Do not delete branches unless the user explicitly asks.
- Do not create releases or tags unless the user explicitly asks.
- Do not change repository settings unless the user explicitly asks.
- Do not add unrelated edits to a PR. Keep each PR limited to its linked issue.
- Do not use `Closes #...`, `Fixes #...`, or `Resolves #...` in draft or experimental PRs unless the PR is intended to close that issue when merged.
- Do not mark work as complete only because code compiles. Check tests, scope, documentation, and issue acceptance criteria.

If a task requires an operation above, stop and ask the user to perform or explicitly authorise it.

## Shell command guidelines

- Avoid command substitution: do not use `$(...)` or backticks.
- Avoid variable expansion (e.g. `$VAR`); use literal values instead.
- Do not chain commands with `&&`, `||`, `;`, or pipes `|`.
  Run each command as a separate tool call.
- Avoid redirection (`>`, `>>`) — use the file edit tools instead.
- Prefer the simplest literal command form so it matches the allowlist.

## GitHub Workflow

Use this workflow for planned work:

1. Start from current `main`.
2. Create one branch per issue.
3. Implement only the scope of that issue.
4. Add or update tests for the acceptance criteria.
5. Update documentation for user-visible behaviour changes.
6. Open a PR linked to the issue.
7. Wait for review.
8. The user merges the PR.

Recommended branch naming:

```text
fix/gh-issue-9-uci-stdout
feat/gh-issue-15-bench-signature
refactor/gh-issue-22-eval-params
ci/gh-issue-26-release-smoke-test
```

Recommended PR body:

```markdown
## Summary
## Linked issue
Closes #
## Tests
- [ ] cmake --build build --parallel
- [ ] ctest --test-dir build --output-on-failure
## Risk
## Notes
```

Use `Closes #N` only when the PR is ready to close the issue on merge.

Before requesting review after a rework, update the PR body so that Summary,
Tests, Risk, and Notes describe the current implementation, not an earlier
attempt. If a review requested a design change, mention how the final patch
addresses it.

## Pull Request Scope Rules

Every PR must be reviewable as a single logical change.

Allowed:

* Code needed for the linked issue.
* Tests for the linked issue.
* Documentation directly affected by the linked issue.
* Small refactors that are necessary for the linked issue.

Not allowed:

* Drive-by formatting of unrelated files.
* Updating AGENTS.md, CLAUDE.md, README.md, or PROGRESS.md unless directly relevant.
* Combining unrelated bug fixes.
* Combining search/eval strength changes with mechanical refactors.
* Combining benchmark harness changes with engine behaviour changes unless required.

If unrelated cleanup is found, split it into a separate PR.

## Post-Rework Review Checklist

Before marking a PR ready for review after substantial changes, check for stale
artefacts left by earlier implementation attempts:

* unused member variables or dead helper functions;
* stale comments describing old behaviour;
* tests that still assert old behaviour;
* PR body text that no longer matches the implementation;
* documentation that still describes previous semantics.

Remove obsolete state rather than keeping it as defensive clutter.

## Build

Out-of-tree build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Produces

* chess-engine — UCI engine using stdin/stdout protocol.
* bench_engine — local perft/search/tactical benchmark runner.

## Test

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

When changing UCI, search timing, async subprocess behaviour, or emitted info
lines, rebuild the relevant executable and repeat the UCI tests:

```bash
cmake --build build --target chess-engine test_uci
ctest --test-dir build --output-on-failure -R uci --repeat-until-fail 3
```

When changing board legality, FEN parsing, hashing, make/unmake, or move
generation, run board/movegen/perft-related tests in addition to the full suite.

When changing search, evaluation, or move ordering, also run:

```bash
./build/bench_engine
./build/bench_engine --json
./build/bench_engine bench
./build/bench_engine bench --json
```

For strength-sensitive changes, do not rely on unit tests alone. Use FastChess /
SPRT or another match framework as required by the issue.

## Async and Timing Test Rules

When a UCI test fails because of `quit`, `stop`, wall-clock delay, or sanitizer
slowdown, do not fix it by only increasing sleeps, delays, or time limits.

First decide whether the expected behaviour should be deterministic. Prefer code
paths that remove races, such as returning the only legal root move immediately,
over tests that depend on CI runner timing.

Timing-based tests must verify protocol invariants, not exact depth counts,
precise elapsed time, or the final info line when a search can finish before
`quit` or `stop` is delivered.

## UCI Protocol Rules

UCI protocol responses must go to stdout:

* id
* option
* uciok
* readyok
* info
* bestmove

Internal diagnostics may go to stderr:

* malformed command diagnostics;
* file-loading failures;
* debug messages not intended for GUI parsing.

Do not emit protocol messages on stderr. This can break GUI and bot integration.

Avoid brittle UCI tests that depend on exact asynchronous info depth line counts
or the final info line when a search can finish before `quit` or `stop` is
delivered. Prefer checking required tokens and using enough delay for CI runners.

## Time Control Semantics

Keep UCI time-control modes explicit. Do not infer clock-managed search from
generic timed-search state such as `!infinite && hardTimeMs > 0`.

Distinguish at least:

* fixed movetime: `go movetime N`;
* clock-managed search: `go wtime ... btime ...`;
* node-limited search: `go nodes N`;
* infinite search: `go infinite`;
* default fallback search.

Adaptive time-management heuristics may only apply to clock-managed searches
unless the issue explicitly says otherwise.

When changing time management, check MultiPV separately. Current MultiPV is
serial: the engine searches one PV line after another using shared timing
context. A time-management change for single-PV search may not be safe for
`MultiPV > 1`. Either test MultiPV explicitly or disable the new adaptive
behaviour for MultiPV until its time-allocation semantics are clear.

## Benchmark and Strength Testing

bench_engine has multiple roles:

```bash
./build/bench_engine                         # built-in perft + search benchmark
./build/bench_engine --json                  # machine-readable built-in benchmark
./build/bench_engine path/to/tactical.epd    # EPD tactical test
./build/bench_engine bench                   # deterministic bench signature mode
./build/bench_engine bench --json            # JSON bench signature output
```

Rules:

* Do not fail CI on exact NPS or elapsed time.
* Deterministic signatures should exclude timing fields.
* If a search/eval change intentionally changes the bench signature, document why.
* Keep benchmark changes separate from engine-strength changes unless the issue requires both.
* For search/eval/move-ordering changes, reduced node count or unchanged tactical EPD is not sufficient evidence of improvement.
* If SPRT is required and the result is negative, accepts H0 with a negative observed score, or otherwise fails the issue acceptance criteria, close the PR unmerged and document the result on the issue.

### SPRT testing workflow

When an issue or PR requires FastChess SPRT testing, use this workflow:

1. **Agent prepares the environment** — create a directory under `/tmp/sprt-<issue>/` with the structure expected by `scripts/run-fastchess-sprt.sh`:
   - `vers/` — baseline engine binary named after its version (usually `main`), and one or more test engine binaries.
   - `books/` — copy the opening book (e.g. `books/8moves_v3.pgn`).
   - `runs/` — empty output directory.
   - Copy `scripts/run-fastchess-sprt.sh` into the test directory.
   - Build baseline and test engines from clean worktrees. Copy only the `chess-engine` UCI binary into `vers/`, not `bench_engine` or test binaries.
   - Record bench signatures for each engine with `bench_engine bench --json`.

2. **Agent provides the command** — the user runs the SPRT, not the agent:

   ```bash
   cd /tmp/sprt-<issue> && ./run-fastchess-sprt.sh main <test-engine-name>
   ```

3. **User runs the command** in their terminal and notifies the agent when the SPRT completes.

4. **Agent inspects results** — after the user notifies, the agent:
   - Finds the run directory under `runs/<timestamp>-main-vs-<engine>/`.
   - Reads `metadata.txt` for engine versions and configuration.
   - Greps `fastchess.log` for the SPRT conclusion.

5. **Agent updates the PR body** with the SPRT result and, if negative, documents it on the linked issue.

#### SPRT modes

Two built-in modes are available, selectable via workflow dispatch or PR label:

| Mode | SPRT args | PR label | Use case |
|------|-----------|----------|----------|
| prove-gain | `elo0=0 elo1=10 alpha=0.10 beta=0.10` | `sprt-prove-gain` | Prove a positive Elo gain |
| non-regression | `elo0=-5 elo1=0 alpha=0.05 beta=0.05` | `sprt-non-regression` | Reject unacceptable regression |

For PR-triggered CI runs, mode is resolved by label:
- `sprt-non-regression` → non-regression mode.
- `sprt-prove-gain` → prove-gain mode.
- no mode label → prove-gain (default).
- `sprt-required` triggers the run but does not select a mode.

For workflow dispatch, mode is selected directly from the `test_mode` input.
A `custom` mode is also available with manually specified SPRT arguments.

#### Interpreting results

The run summary includes an interpretation section:

- **pass** — H1 accepted (improvement detected).
- **fail** — H0 accepted (no improvement / regression detected).
- **inconclusive** — SPRT did not reach a terminal boundary (cancelled, timeout, or still running).

Do not merge a PR with an inconclusive or failed SPRT result.

## Lint

If available:

```bash
clang-tidy src/**/*.cpp -- -std=c++17 -I src
```

Do not introduce a new lint dependency unless the issue calls for it.

## Code Conventions

* Language: C++17.
* Indentation: 4 spaces.
* Braces: opening brace on the same line.
* Headers: use #pragma once.
* Classes/types: PascalCase.
* Functions/methods: camelCase.
* Member variables: camelCase_ with trailing underscore.
* Constants/enums: UPPER_SNAKE_CASE.
* Prefer stack allocation and RAII.
* Avoid raw new / delete.
* Keep external dependencies minimal.
* Avoid exceptions in engine code; prefer return codes, booleans, and std::optional.

## Testing Conventions

* Unit tests live under test/.
* Use the existing simple assert-style test approach.
* Add regression tests for every bug fix.
* For parser/loader changes, test both accepted and rejected inputs.
* For invalid input, test that previous valid state is preserved.
* For UCI changes, test the actual protocol output, not only internal functions.
* For time-management changes, test fixed movetime, clock-managed search, low-clock behaviour, and `searchmoves` restrictions separately when relevant.

## Documentation Rules

Update documentation when changing:

* user-visible UCI options;
* command-line tool behaviour;
* benchmark output;
* release artefacts;
* opening-book semantics;
* build/test instructions.

Do not update documentation unrelated to the issue in the same PR.

## Commit Guidelines

Use focused commits with clear messages.

Examples:

```text
fix: send book info string to stdout
fix: parse go nodes as uint64
feat: add deterministic bench signature mode
docs: clarify Book Max Ply semantics
ci: add release smoke test
```

Avoid vague messages:

```text
update
fix stuff
changes
```

## Key Modules

| Module     | Files                                        | Purpose                                                                 |
| ---------- | -------------------------------------------- | ----------------------------------------------------------------------- |
| Types      | src/engine/types.h, src/engine/types.cpp     | Bitboard, square, piece, move, constants                                |
| Attacks    | src/engine/attacks.h, src/engine/attacks.cpp | Magic bitboard slider attacks and precomputed tables                    |
| Board      | src/engine/board.h, src/engine/board.cpp     | Position, FEN, Zobrist hash, make/unmake, incremental eval              |
| Move Gen   | src/engine/movegen.h, src/engine/movegen.cpp | Legal move generation, perft, check/pin/castling/en-passant logic       |
| Evaluation | src/engine/eval.h, src/engine/eval.cpp       | Classical tapered evaluation                                            |
| SEE        | src/engine/see.h, src/engine/see.cpp         | Static exchange evaluation                                              |
| Search     | src/engine/search.h, src/engine/search.cpp   | PVS alpha-beta, iterative deepening, quiescence, pruning, move ordering |
| TT         | src/engine/tt.h, src/engine/tt.cpp           | Transposition table                                                     |
| Book       | src/engine/book.h, src/engine/book.cpp       | Polyglot opening book                                                   |
| UCI        | src/engine/uci.h, src/engine/uci.cpp         | UCI protocol, options, time management, MultiPV, WDL                    |
| Bench      | tools/bench.cpp                              | Perft/search/EPD/bench-signature tooling                                |
| Poly Keys  | src/engine/poly_keys.h                       | Polyglot Zobrist constants                                              |

