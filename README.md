# README.md

A C++ chess engine built from scratch with UCI protocol support.

## Architecture

```
src/engine/               # Chess engine core
├── types.h/cpp           #   Square, Piece, Color, Move, Bitboard, helpers
├── attacks.h/cpp         #   Magic bitboard attack tables (all piece types)
├── board.h/cpp           #   Bitboard board, FEN, make/unmake, Zobrist hash, incremental eval state
├── movegen.h/cpp         #   Check/pin-aware legal move generation (perft-verified)
├── eval.h/cpp            #   Tapered PeSTO eval + pawn/eval caches, mobility, king safety
├── see.h/cpp             #   Static exchange evaluation for move ordering and pruning
├── search.h/cpp          #   Alpha-beta PVS + staged move picking + LMR + null move pruning
├── tt.h/cpp              #   4-way clustered, generation-aware TT with static eval storage (16B entries)
├── book.h/cpp            #   Polyglot opening book loader (.bin format, weighted random)
├── uci.h/cpp             #   UCI protocol handler (WDL support, time management)
└── poly_keys.h           #   Polyglot Zobrist key constants (header-only)
```

## Dependencies

| Tool | Purpose | Install |
|------|---------|---------|
| CMake >= 3.16 | Build system | `brew install cmake` |

## Quick Start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)  # Linux: use $(nproc)
./chess-engine               # Run as UCI engine
```

Use in any UCI-compatible chess GUI (CuteChess, Arena, etc.).

## Lichess Bot Deployment

To deploy as a Lichess bot using the official [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot) client:

```bash
./scripts/setup-lichess-bot-runtime.sh /path/to/deployment
```

This builds the engine, clones lichess-bot, creates a Python venv, writes `config.yml`, and validates UCI startup. Supports environment overrides: `LICHESS_TOKEN`, `LICHESS_BOT_REPO`, `LICHESS_BOT_REF`, `PYTHON_BIN`, etc.

## Opening Book

The engine supports Polyglot (.bin) opening books. Use UCI setoption:

```
setoption name OwnBook value true
setoption name Book File value books/gm2001.bin
setoption name Book Max Ply value 10
# Book Max Ply limits book probing to the first N half-moves (plies).
# 0 disables the book; 1 allows only the root position.
setoption name Book Random value true
```

### Getting a Book

Pre-built Polyglot books: https://github.com/michaeldv/polyglot_books

Or generate from a PGN database:

```bash
polyglot make-book -pgn games.pgn -bin mybook.bin -max-ply 20
```

## Benchmarking

Run the lightweight in-repo benchmark target after building:

```bash
./bench_engine                         # built-in perft + search benchmark
./bench_engine --json                  # same, machine-readable JSON
./bench_engine path/to/tactical.epd    # EPD best-move test
./bench_engine bench                   # deterministic bench signature mode
./bench_engine bench --json            # bench signature, JSON for CI
```

### Bench Signature

`bench_engine bench` runs fixed-depth (depth 8) searches over 15 committed FENs
covering opening, middlegame, and endgame positions. It emits a deterministic
FNV-64 signature over the node counts and bestmove strings of every position.

- The signature depends only on nodes and bestmoves — not on elapsed time or NPS.
- Re-running the same binary with the same configuration produces the identical
  bestmove list and signature.
- JSON output is machine-readable and intended for CI regression detection.

## Features

- Magic bitboard move generation with check/pin awareness
- Tapered evaluation with PeSTO piece-square tables, pawn structure, mobility, king safety
- PVS alpha-beta search with iterative deepening, aspiration windows, null-move pruning, LMR
- Staged move ordering: TT move, SEE-filtered captures, killer/countermove, history heuristic
- 4-way clustered transposition table with generation aging and static eval storage
- Polyglot opening book support (weighted random or deterministic)
- UCI protocol with time management, MultiPV, WDL reporting
- Incremental material/PST evaluation with pawn hash and eval cache
- Deterministic make/unmake with full state rollback (hash, eval, pawn hash, repetition)

## Roadmap

- Automated classical eval tuning (Texel/SPSA)
- Fixed-node/depth tactical EPD and speed benchmark suites
- Optional Syzygy endgame tablebase probing
- NNUE evaluation experiment
- SMP search with UCI `Threads`

## SPRT

SPRT (Sequential Probability Ratio Testing) regression tests compare the current build against a frozen baseline to detect strength regressions:

```bash
# Compare two versioned engine binaries under scripts/vers/
./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION

# Custom parameters
TC=5+0.05 ROUNDS=20000 CONCURRENCY=4 ./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION
```

Results are written under `scripts/runs/<timestamp>-OLD_VERSION-vs-NEW_VERSION/`.

Environment variables: `TC`, `ROUNDS`, `GAMES`, `SPRT`, `CONCURRENCY`, `STARTUP_MS`, `UCINEWGAME_MS`.

## License

MIT
