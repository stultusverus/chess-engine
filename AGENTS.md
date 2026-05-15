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

```
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

## Pull Request Scope Rules

Every PR must be reviewable as a single logical change.

Allowed:

* Code needed for the linked issue.
* Tests for the linked issue.
* Documentation directly affected by the linked issue.
* Small refactors that are necessary for the linked issue.

Not allowed:

* Drive-by formatting of unrelated files.
* Updating AGENTS.md, README.md, or PROGRESS.md unless directly relevant.
* Combining unrelated bug fixes.
* Combining search/eval strength changes with mechanical refactors.
* Combining benchmark harness changes with engine behaviour changes unless required.

If unrelated cleanup is found, split it into a separate PR.

## Build

Out-of-tree build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Produces:

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
or the final info line when a search can finish before quit or stop is
delivered. Prefer checking required tokens and using enough delay for CI runners.

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

```
fix: send book info string to stdout
fix: parse go nodes as uint64
feat: add deterministic bench signature mode
docs: clarify Book Max Ply semantics
ci: add release smoke test
```

Avoid vague messages:

update
fix stuff
changes

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
