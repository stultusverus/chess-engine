# README.md

A C++ chess engine built from scratch that plays as a bot on [Lichess.org](https://lichess.org) via the [Bot API](https://lichess.org/api#tag/Bot).

## Architecture

```
src/
├── engine/               # Chess engine core
│   ├── types.h/cpp       #   Square, Piece, Color, Move, Bitboard, helpers
│   ├── attacks.h/cpp     #   Magic bitboard attack tables (all piece types)
│   ├── board.h/cpp       #   Bitboard board, FEN, make/unmake, Zobrist hash
│   ├── movegen.h/cpp     #   Legal move generation (perft-verified)
│   ├── eval.h/cpp        #   Tapered PeSTO evaluation (material + PST)
│   ├── search.h/cpp      #   Alpha-beta PVS + iterative deepening + LMR
│   ├── tt.h/cpp          #   Transposition table (always-replace)
│   ├── book.h/cpp        #   Polyglot opening book loader (.bin format)
│   └── uci.h/cpp         #   UCI protocol handler
├── bot/                  # Lichess bot client
│   ├── client.h/cpp      #   HTTP client (libcurl), NDJSON streaming, all API calls
│   ├── manager.h/cpp     #   Event loop, challenge policy, game orchestration
│   └── decision.h        #   Auto-resign/draw decision logic (pure, testable)
├── main.cpp              #   UCI engine entry point
└── bot_main.cpp          #   Bot entry point
```

## Dependencies

| Library | Purpose | Install |
|---------|---------|---------|
| CMake >= 3.16 | Build system | `brew install cmake` |
| libcurl | HTTP requests | `brew install curl` |
| nlohmann/json | JSON/NDJSON parsing | auto-fetched by CMake |

## Quick Start

### 1. Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

### 2. Get a Lichess Bot Token

1. Create a Lichess account for your bot
2. Go to https://lichess.org/account/oauth/token/create?scopes[]=bot:play
3. Generate a token with "Play bot moves" scope
4. Upgrade to bot: `curl -d '' https://lichess.org/api/bot/account/upgrade -H "Authorization: Bearer TOKEN"`
5. Save the token to `.lichess.key` in the project root

### 3. Run

```bash
# As Lichess bot (reads token from .lichess.key, LICHESS_TOKEN env, or --token)
./build/chess-bot

# As UCI engine (for chess GUIs like CuteChess, Arena)
./build/chess-engine
```

## Bot Configuration

```
chess-bot [options]
  --token TOKEN         Lichess API token (env: LICHESS_TOKEN, file: .lichess.key)
  --book FILE           Path to Polyglot opening book (.bin)
  --debug               Verbose logging (HTTP requests, stream events, search info)
  --challenge USER      Challenge a specific player then wait for the game
  --challenge-bots N    Fetch online bots and challenge up to N of them
  --rated-only          Only accept rated games
  --min-time N          Minimum clock seconds to accept (default: 30)
  --max-time N          Maximum clock seconds to accept (default: 1800)
  --min-increment N     Minimum clock increment to accept (default: 0)
  --no-resign           Disable auto-resign
  --resign-threshold N  Resign eval threshold in centipawns (default: -800)
  --no-draw             Disable auto-draw offer/accept
  --draw-threshold N    Draw eval threshold in centipawns (default: 20)
```

## Opening Book

The engine supports Polyglot (.bin) opening books. Place a `.bin` file in the `books/` directory and use the `--book` flag:

```bash
./build/chess-bot --token TOKEN --book books/gm2001.bin
```

For the UCI engine, use setoption:

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

## License

MIT
