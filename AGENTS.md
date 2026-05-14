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

When changing UCI/search timing, async subprocess tests, or anything that affects emitted `info` lines, rebuild the relevant executable and run the UCI test more than once before committing:

```bash
cd build
cmake --build . --target chess-engine test_uci
ctest --output-on-failure -R uci --repeat-until-fail 3
```

Avoid brittle UCI tests that depend on exact asynchronous `info depth` line counts or the last info line when a search can finish before `quit`/`stop` is delivered. Prefer asserting the specific required token appears with enough delay for CI runners.

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

## Pull Requests

- **Never merge PRs.** The user handles all merging.

## Project Goal

A C++ chess engine built from scratch (bitboard representation, alpha-beta search, UCI protocol).

## Key Modules

| Module | Files | Purpose |
|--------|-------|---------|
| Types | `types.h/cpp` | Bitboard, Square, Piece, Move, constants |
| Attacks | `attacks.h/cpp` | Magic bitboard slider attacks, precomputed tables |
| Board | `board.h/cpp` | Position, FEN, Zobrist hash, make/unmake, incremental eval |
| Move Gen | `movegen.h/cpp` | Check/pin-aware legal move generation, perft |
| Evaluation | `eval.h/cpp` | Tapered PeSTO eval, pawn structure, mobility, king safety |
| SEE | `see.h/cpp` | Static exchange evaluation for move ordering and pruning |
| Search | `search.h/cpp` | PVS alpha-beta, iterative deepening, quiescence, LMR, null-move |
| TT | `tt.h/cpp` | 4-way clustered transposition table with generation aging |
| Book | `book.h/cpp` | Polyglot opening book (weighted random or deterministic) |
| UCI | `uci.h/cpp` | UCI protocol, time management, MultiPV, WDL |
| Poly Keys | `poly_keys.h` | Polyglot Zobrist constants (header-only) |
