# Chess Engine

A C++ chess engine built from scratch that plays as a bot on [Lichess.org](https://lichess.org) via the [Bot API](https://lichess.org/api#tag/Bot).

## Architecture

```
┌──────────────────────────────────────────┐
│               main.cpp                    │
│        (config, signal handling)          │
├──────────────────────────────────────────┤
│           bot/                            │
│  ┌────────┐ ┌────────┐ ┌──────────────┐  │
│  │ client │ │ stream │ │   manager    │  │
│  │ (HTTP) │ │(NDJSON)│ │(challenges,  │  │
│  │        │ │        │ │ game routing)│  │
│  └────────┘ └────────┘ └──────────────┘  │
├──────────────────────────────────────────┤
│          engine/                          │
│  ┌──────┐ ┌───────┐ ┌──────┐ ┌────────┐ │
│  │board │ │movegen│ │ eval │ │ search │ │
│  └──────┘ └───────┘ └──────┘ └────────┘ │
│  ┌──────┐ ┌───────┐                     │
│  │  tt  │ │  uci  │                      │
│  └──────┘ └───────┘                     │
└──────────────────────────────────────────┘
```

- **Engine core**: Bitboard board representation, magic bitboard move generation, alpha-beta search with iterative deepening, quiescence search, transposition table, UCI protocol.
- **Bot client**: HTTP via libcurl, NDJSON event/game stream parsing, challenge filtering, concurrent game management.

## Dependencies

| Library | Purpose | Install |
|---------|---------|---------|
| CMake >= 3.16 | Build system | `brew install cmake` |
| libcurl | HTTP requests | `brew install curl` |
| nlohmann/json | JSON/NDJSON parsing | Header-only (vendored or `brew install nlohmann-json`) |

## Quick Start

### 1. Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 2. Get a Lichess Bot Token

1. Create a Lichess account for your bot
2. Go to https://lichess.org/account/oauth/token/create?scopes[]=bot:play
3. Generate a token with the "Play bot moves" scope
4. Upgrade the account to a bot: `curl -d '' https://lichess.org/api/bot/account/upgrade -H "Authorization: Bearer YOUR_TOKEN"`

### 3. Run

```bash
./build/chess-engine --token YOUR_LICHESS_TOKEN
```

The bot will appear online at https://lichess.org/player/bots and accept challenges matching its configured criteria.

## Configuration

Command-line options:

| Flag | Description | Default |
|------|-------------|---------|
| `--token` | Lichess API token (required) | — |
| `--min-time` | Minimum clock seconds to accept | 60 |
| `--max-time` | Maximum clock seconds to accept | 1800 |
| `--min-increment` | Minimum increment to accept | 0 |
| `--rated-only` | Only accept rated games | false |
| `--resign-threshold` | Eval in centipawns to auto-resign | -800 |
| `--draw-threshold` | Eval range for accepting draws | ±20 |
| `--max-threads` | Search threads | 1 |
| `--hash` | Transposition table size (MB) | 64 |

## License

MIT
