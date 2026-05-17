# README.md

A C++ chess engine built from scratch with UCI protocol support.

## Architecture

```
src/engine/               # Chess engine core
├── types.h/cpp           #   Square, Piece, Color, Move, Bitboard, helpers
├── attacks.h/cpp         #   Magic bitboard attack tables (all piece types)
├── board.h/cpp           #   Bitboard board, FEN, make/unmake, Zobrist hash, incremental eval state
├── movegen.h/cpp         #   Check/pin-aware legal move generation (perft-verified)
├── eval.h/cpp            #   Tapered PeSTO eval with pawn/eval caches, mobility, king safety
├── eval_params.h         #   Tunable evaluation parameters (centralised for Texel/SPSA)
├── see.h/cpp             #   Static exchange evaluation for move ordering and pruning
├── search.h/cpp          #   Alpha-beta PVS with aspiration windows, LMR, LMP, null-move pruning
├── tt.h/cpp              #   4-way clustered, generation-aware TT with static eval storage (16B entries)
├── book.h/cpp            #   Polyglot opening book loader (.bin format, weighted random)
├── uci.h/cpp             #   UCI protocol handler (WDL support, time management, MultiPV)
├── tune.h/cpp            #   Texel tuning scaffold (dataset loader, MSE loss, K-parameter opt)
└── poly_keys.h           #   Polyglot Zobrist key constants (header-only)
```

## Dependencies

| Tool | Purpose | Install |
|------|---------|---------|
| CMake >= 3.16 | Build system | `brew install cmake` |

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/chess-engine         # Run as UCI engine
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
./build/bench_engine                         # built-in perft + search benchmark
./build/bench_engine --json                  # same, machine-readable JSON
./build/bench_engine test/fixtures/tactical.epd  # EPD tactical test
./build/bench_engine trace <FEN> [--json]    # eval trace (per-term breakdown)
./build/bench_engine bench                   # deterministic bench signature mode
./build/bench_engine bench --json            # bench signature, JSON for CI
```

### Bench Signature

`bench_engine bench` runs fixed-depth (depth 8) searches over 15 committed FENs
covering opening, middlegame, and endgame positions. It emits a deterministic
FNV-64 signature over the node counts and bestmove strings of every position.

- The signature depends only on nodes and bestmoves — not on elapsed time or NPS.
- Re-running the same binary with the same configuration produces the identical
  bestmove list and signature.
- JSON output is machine-readable and intended for CI regression detection.

### Tuning Scaffold

`bench_engine tune-eval` provides a minimal Texel tuning scaffold for offline
evaluation parameter tuning. It reads a dataset of positions and game results,
computes a baseline mean-squared-error loss, and performs a lightweight
K-parameter optimisation as a proof of concept.

```bash
./build/bench_engine tune-eval path/to/dataset.txt       # human-readable report
./build/bench_engine tune-eval path/to/dataset.txt --json # machine-readable JSON
```

#### Dataset Format

One position per line in `<fen> <result>` format:

```text
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2
8/8/8/4k3/4P3/4K3/8/8 w - - 0 1 1-0
8/8/8/4k3/4p3/4K3/8/8 w - - 0 1 0-1
```

Results must be `1-0` (white win), `1/2-1/2` (draw), or `0-1` (black win).
Leading/trailing whitespace is trimmed. Blank lines and lines starting with
`#` are ignored. Malformed lines are reported as warnings and skipped.

The committed fixture at `test/fixtures/tune-small.txt` is a smoke-test dataset
for validating the scaffold, not a real tuning corpus. Real tuning requires a
larger dataset of quiet positions from actual games.

The scaffold does **not** change engine evaluation parameters. It reports
baseline loss (K=1.0) and the best K found via golden-section search. All
output is deterministic for a fixed dataset. Future work may extend this to
multi-parameter Texel or SPSA tuning with separate review and SPRT.

## Features

### Search
- PVS alpha-beta with iterative deepening
- Aspiration windows from depth 4 with retry on fail-high/fail-low
- Null-move pruning with mate-guard verification (R=3)
- Late-move reductions (LMR) with PV/non-PV distinction
- Late-move pruning (LMP) at shallow non-PV nodes for quiet non-checking moves
- Futility pruning at depths 1–2
- Adaptive time management with stability and score-drop detection
- Staged move ordering: TT move → SEE-filtered good captures → killer/countermove → history heuristic → bad captures
- Countermove and continuation history (butterfly + piece-square + capture history)
- Node-limited search (`go nodes N`)

### Evaluation
- Tapered PeSTO evaluation with piece-square tables
- Pawn structure: doubled, isolated, passed pawns
- Mobility (per-piece multipliers by game phase)
- King safety: shield, open-file penalty, attacker proximity
- Bishop pair, rook on (semi-)open file, tempo bonus
- Eval trace mode for per-term breakdown
- Centralised `EvalParams` struct for Texel/SPSA tuning

### Transposition Table
- 4-way clustered, depth-preferred replacement with generation aging
- Static eval storage in TT entries (16 bytes per entry)

### UCI Protocol
- Clock-managed, fixed movetime, node-limited, and infinite search modes
- MultiPV (1–4 lines)
- WDL reporting with sigmoid approximation
- Polyglot opening book support (weighted random or deterministic)
- `searchmoves` root move restriction

## Roadmap

- Multi-parameter Texel/SPSA eval tuning (scaffold in place, K-only today)
- Syzygy endgame tablebase probing
- NNUE evaluation experiment
- SMP search with UCI `Threads`

## SPRT

SPRT (Sequential Probability Ratio Testing) tests compare the current build against a frozen baseline. Two modes are supported:

- **prove-gain** (`elo0=0 elo1=10 alpha=0.10 beta=0.10`): prove a positive Elo gain.
- **non-regression** (`elo0=-5 elo1=0 alpha=0.05 beta=0.05`): reject unacceptable regression, useful for refactors, performance work, and correctness fixes that should not hurt strength.

The CI workflow (`.github/workflows/sprt.yml`) can be triggered manually via workflow dispatch with mode selection, or automatically on PRs labelled `sprt-required`. PR labels `sprt-prove-gain` or `sprt-non-regression` select the mode; the default is `prove-gain`.

```bash
# Compare two versioned engine binaries
./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION

# Custom parameters
TC=5+0.05 ROUNDS=20000 CONCURRENCY=4 ./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION

# Non-regression mode
SPRT_MODE=non-regression ./scripts/run-fastchess-sprt.sh OLD_VERSION NEW_VERSION
```

Results are written under `runs/<timestamp>-OLD_VERSION-vs-NEW_VERSION/`.

Environment variables: `TC`, `ROUNDS`, `GAMES`, `SPRT`, `SPRT_MODE`, `DRAW`, `RESIGN`, `CONCURRENCY`, `STARTUP_MS`, `UCINEWGAME_MS`, `PING_MS`, `HASH_MB`.

## License

MIT
