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
├── search.h/cpp          #   Alpha-beta PVS + iterative deepening + LMR + null move pruning
├── tt.h/cpp              #   Depth-preferred transposition table (16B entries)
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
./bench_engine
./bench_engine --json
./bench_engine path/to/tactical.epd
./bench_engine --json path/to/tactical.epd
```

## Current Development Status

Recently fixed review items:

- Root `searchmoves` and serial `MultiPV` no longer return moves outside the restricted root set due to TT hits.
- Opening-book probing respects `searchmoves`, skips analysis-style searches, and validates book moves.
- En-passant FEN and hashing are normalized so non-capturable EP targets can round-trip without splitting TT/repetition keys.
- `Ponder` is not advertised until true ponder continuation is implemented.
- Quiescence search uses dedicated noisy-move generation for legal captures, en-passant captures, and promotions outside check.
- Legal move generation is check/pin-aware, and board make/unmake maintains incremental material/PST state, pawn hash, and eval-cache inputs.
- Null-move en-passant hash removal is consistent with the old side to move.
- The UCI `Hash` option is an allocation cap; TT entries round down to a power of two.
- Serial `MultiPV` searches share the same time origin for time-managed searches.
- Unsupported `go ponder` returns `bestmove 0000` with an `info string` diagnostic.

High-value strength/performance work:

- Staged move picker with lazy SEE for captures.
- Clustered/generation-aware TT replacement and optional static-eval storage.
- Automated classical eval tuning with Texel/SPSA.
- Fixed-node/depth tactical EPD and speed benchmark suites.
- Optional Syzygy probing, NNUE experiments, and eventually SMP/`Threads`.

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
