# AGENTS.md

Instructions for AI coding agents working on this codebase.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Produces two executables:
- `chess-engine` — UCI engine (stdin/stdout protocol)
- `chess-bot` — Lichess bot (connects automatically, reads token from `.lichess.key`, `LICHESS_TOKEN` env, or `--token` flag)

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
- **Dependencies**: Keep external deps minimal — libcurl, nlohmann/json (header-only). All other code is in-repo.
- **No exceptions**: Use return codes / `std::optional` for error handling.
- **Testing**: Unit tests go in `test/` matching `src/` structure. Use Catch2 or simple assert-based tests.

## Commits

- **Commit frequently.** After completing each logical unit of work (a new file, a completed module, a passing test), create a commit with a clear message.

## Project Goal

A C++ chess engine that plays as a bot on Lichess.org via the Bot API. The engine core is built from scratch (bitboard representation, alpha-beta search, UCI protocol). A bot client layer handles the Lichess REST API and NDJSON streaming for event/game streams.

## Bot Debugging

```bash
# Run bot with verbose output: HTTP requests, stream events, search info
./build/chess-bot --debug

# Challenge a specific bot
./build/chess-bot --challenge SomeBotName

# Challenge random online bots
./build/chess-bot --challenge-bots 3

# Run bot with an opening book
./build/chess-bot --book books/gm2001.bin --debug
```
