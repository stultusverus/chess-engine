# AGENTS.md

Instructions for AI coding agents working on this codebase.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)  # Linux: use $(nproc)
```

Produces one executable:
- `chess-engine` — UCI engine (stdin/stdout protocol)

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
- **Dependencies**: Keep external deps minimal — nlohmann/json (header-only). All other code is in-repo.
- **No exceptions**: Use return codes / `std::optional` for error handling.
- **Testing**: Unit tests go in `test/` matching `src/` structure. Use simple assert-based tests (no test framework dependency).

## Commits

- **Commit frequently.** After completing each logical unit of work (a new file, a completed module, a passing test), create a commit with a clear message.

## Project Goal

A C++ chess engine built from scratch (bitboard representation, alpha-beta search, UCI protocol).

## Opening Book

The engine supports Polyglot (.bin) opening books. Use UCI setoption:

```
setoption name OwnBook value true
setoption name Book File value books/gm2001.bin
```

## SPRT Regression Testing

Compare current build against the frozen baseline for non-regression:

```bash
# Default: 10+0.1s time control, all CPU cores
./bench/run-fastchess-sprt.sh

# Custom time control and rounds
TC=5+0.05 ROUNDS=20000 ./bench/run-fastchess-sprt.sh

# Lower concurrency
CONCURRENCY=4 ./bench/run-fastchess-sprt.sh
```

Environment variables: `TC`, `ROUNDS`, `GAMES`, `SPRT`, `CONCURRENCY`, `STARTUP_MS`.

## Runtime Deployment

Set up a fully-configured lichess-bot deployment with a single script (uses the official lichess-bot client):

```bash
./scripts/setup-lichess-bot-runtime.sh /path/to/deployment
```

This builds the engine, clones lichess-bot, creates a Python venv, writes `config.yml`, and validates UCI startup.
