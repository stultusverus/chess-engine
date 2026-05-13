# AGENTS.md

Instructions for AI coding agents working on this codebase.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)  # Linux: use $(nproc)
```

Produces two executables:
- `chess-engine` — UCI engine (stdin/stdout protocol)
- `bench_engine` — local perft/search/tactical benchmark runner; pass `--json` for machine-readable output

## Test

```bash
cd build && ctest --output-on-failure
```

## Lint

```bash
# clang-tidy (if available)
clang-tidy src/**/*.cpp -- -std=c++17 -I src
```

## Conventions

- **Language**: C++17
- **Style**: 4-space indentation. Opening brace on same line. LowerCamelCase for functions and methods, PascalCase for classes and types.
- **Headers**: Use `#pragma once`. One class per header/source pair.
- **Naming**:
  - Classes/Types: `PascalCase` (e.g., `MoveGenerator`, `GameManager`)
  - Functions/Methods: `camelCase` (e.g., `makeMove`, `generateMoves`)
  - Member variables: `camelCase_` with trailing underscore
  - Constants/enums: `UPPER_SNAKE_CASE`
- **Memory**: Prefer stack allocation and RAII. Avoid raw `new`/`delete`.
- **Logging**: Use `std::cerr` for engine debug output (UCI dictates stderr for info). Use `std::cout` only for UCI responses.
- **Dependencies**: Keep external deps minimal. Current engine code is in-repo plus the C++ standard library.
- **No exceptions**: Use return codes / `std::optional` for error handling.
- **Testing**: Unit tests go in `test/` matching `src/` structure. Use simple assert-based tests (no test framework dependency).

## Commits

- **Commit frequently.** After completing each logical unit of work (a new file, a completed module, a passing test), create a commit with a clear message.

## Project Goal

A C++ chess engine built from scratch (bitboard representation, alpha-beta search, UCI protocol).

## Recent Review Fixes

The latest correctness review fixes are now part of the codebase and should remain covered by regression tests:

- Root `searchmoves` and serial `MultiPV` are protected from transposition-table root hits returning moves outside the active root set.
- Opening-book probing is filtered by `searchmoves`, disabled for analysis-style searches, and validates book moves before returning them.
- En-passant FEN/hash handling accepts syntactically valid EP targets while hashing EP only when an EP capture is possible.
- `Ponder` is no longer advertised until true ponder continuation is implemented.
- Quiescence search uses a dedicated noisy-move generator outside check instead of generating all legal moves and filtering.
- Legal move generation is check/pin-aware.
- Board make/unmake maintains incremental material/PST state, pawn hash, and eval-cache inputs.
- Null-move en-passant hash removal uses the old side to move before toggling side.
- UCI `Hash` sizing is treated as an upper bound by rounding TT entries down to a power of two.
- Serial `MultiPV` searches use a shared time origin for time-managed searches.
- Internal iterative deepening is restricted to deeper PV nodes.
- Unsupported `go ponder` returns `bestmove 0000` with an `info string` diagnostic.
- Transposition table storage uses 4-way clustered buckets with depth/exact-preferred replacement.

Current high-value performance work:

- Add a staged move picker and compute SEE lazily instead of scoring every move up front.
- Improve TT quality further with generation/age and optionally static eval storage.
- Add automated eval tuning before expanding hand-tuned classical terms.
- Extend benchmark coverage with fixed-node/depth tactical EPD and speed suites.

## Opening Book

The engine supports Polyglot (.bin) opening books. Use UCI setoption:

```
setoption name OwnBook value true
setoption name Book File value books/gm2001.bin
setoption name Book Max Ply value 10
```

## SPRT Regression Testing

Compare current build against the frozen baseline for non-regression:

```bash
# Default: 10+0.1s time control, all CPU cores
./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION

# Custom time control and rounds
TC=5+0.05 ROUNDS=20000 ./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION

# Lower concurrency
CONCURRENCY=4 ./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION
```

Environment variables: `TC`, `ROUNDS`, `GAMES`, `SPRT`, `CONCURRENCY`, `STARTUP_MS`.

## Runtime Deployment

Set up a fully-configured lichess-bot deployment with a single script (uses the official lichess-bot client):

```bash
./scripts/setup-lichess-bot-runtime.sh /path/to/deployment
```

This builds the engine, clones lichess-bot, creates a Python venv, writes `config.yml`, and validates UCI startup.
