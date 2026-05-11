#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/setup-lichess-bot-runtime.sh <base-dir>

Creates this layout:
  <base-dir>/
    lichess-bot/        cloned lichess-bot checkout with venv
    runtime/
      config.yml        private lichess-bot config
      books/Titans.bin  runtime copy of the opening book
      engine-workdir/   engine working directory
      logs/             lichess-bot logs
      pgn/              saved game PGNs

Environment overrides:
  LICHESS_TOKEN         token written into runtime/config.yml
  LICHESS_BOT_REPO     repo to clone
  LICHESS_BOT_REF      optional branch/tag/commit to checkout
  PYTHON_BIN           Python executable for venv creation
  BUILD_JOBS           cmake build parallelism
  OVERWRITE_CONFIG=1   replace existing runtime/config.yml

The script validates the generated config and direct UCI engine startup, but it
does not connect to Lichess or start live games.
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 1 ]]; then
    usage
    exit 2
fi

BASE_DIR="$1"
mkdir -p "${BASE_DIR}"
BASE_DIR="$(cd "${BASE_DIR}" && pwd)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ENGINE_ROOT}/build"
BOT_DIR="${BASE_DIR}/lichess-bot"
RUNTIME_DIR="${BASE_DIR}/runtime"
CONFIG_FILE="${RUNTIME_DIR}/config.yml"
BOOK_SRC="${ENGINE_ROOT}/books/Titans.bin"
BOOK_DST="${RUNTIME_DIR}/books/Titans.bin"

LICHESS_BOT_REPO="${LICHESS_BOT_REPO:-https://github.com/lichess-bot-devs/lichess-bot.git}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
BUILD_JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"
TOKEN="${LICHESS_TOKEN:-PUT_LICHESS_BOT_TOKEN_HERE}"

yaml_escape() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

echo "==> Creating runtime directories under ${BASE_DIR}"
mkdir -p \
    "${RUNTIME_DIR}/books" \
    "${RUNTIME_DIR}/engine-workdir" \
    "${RUNTIME_DIR}/logs" \
    "${RUNTIME_DIR}/pgn"

echo "==> Building chess-engine"
cmake -S "${ENGINE_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"

echo "==> Setting up lichess-bot"
if [[ -d "${BOT_DIR}/.git" ]]; then
    echo "    Reusing existing checkout: ${BOT_DIR}"
elif [[ -e "${BOT_DIR}" && -n "$(find "${BOT_DIR}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
    echo "error: ${BOT_DIR} exists and is not an empty git checkout" >&2
    exit 1
else
    git clone "${LICHESS_BOT_REPO}" "${BOT_DIR}"
fi

if [[ -n "${LICHESS_BOT_REF:-}" ]]; then
    git -C "${BOT_DIR}" fetch --all --tags
    git -C "${BOT_DIR}" checkout "${LICHESS_BOT_REF}"
fi

echo "==> Creating Python venv and installing lichess-bot requirements"
if [[ ! -x "${BOT_DIR}/venv/bin/python" ]]; then
    "${PYTHON_BIN}" -m venv "${BOT_DIR}/venv"
fi
"${BOT_DIR}/venv/bin/python" -m pip install --upgrade pip
"${BOT_DIR}/venv/bin/python" -m pip install -r "${BOT_DIR}/requirements.txt"

echo "==> Copying runtime books"
if [[ ! -f "${BOOK_SRC}" ]]; then
    echo "error: missing ${BOOK_SRC}" >&2
    exit 1
fi
cp -f "${BOOK_SRC}" "${BOOK_DST}"

if [[ -f "${CONFIG_FILE}" && "${OVERWRITE_CONFIG:-0}" != "1" ]]; then
    echo "==> Keeping existing ${CONFIG_FILE}"
else
    echo "==> Writing ${CONFIG_FILE}"
    TOKEN_ESCAPED="$(yaml_escape "${TOKEN}")"
    cat > "${CONFIG_FILE}" <<EOF_CONFIG
token: "${TOKEN_ESCAPED}"
url: "https://lichess.org/"

engine:
  dir: "${BUILD_DIR}"
  name: "chess-engine"
  working_dir: "${RUNTIME_DIR}/engine-workdir"
  protocol: "uci"
  debug: true
  ponder: false

  polyglot:
    enabled: true
    book:
      standard:
        - "${BOOK_DST}"
    min_weight: 1
    selection: "weighted_random"
    max_depth: 20
    normalization: "none"

  draw_or_resign:
    resign_enabled: false
    resign_score: -1000
    resign_for_egtb_minus_two: true
    resign_moves: 3
    offer_draw_enabled: true
    offer_draw_score: 0
    offer_draw_for_egtb_zero: true
    offer_draw_moves: 10
    offer_draw_pieces: 10

  online_moves:
    max_out_of_book_moves: 10
    max_retries: 2
    chessdb_book:
      enabled: false
      min_time: 20
      max_time: 10800
      move_quality: "good"
      min_depth: 20
    lichess_cloud_analysis:
      enabled: false
      min_time: 20
      max_time: 10800
      move_quality: "best"
      max_score_difference: 50
      min_depth: 20
      min_knodes: 0
    lichess_opening_explorer:
      enabled: false
      min_time: 20
      max_time: 10800
      source: "masters"
      player_name: ""
      sort: "winrate"
      min_games: 10
    online_egtb:
      enabled: false
      min_time: 20
      max_time: 10800
      max_pieces: 8
      source: "lichess"
      move_quality: "best"

  lichess_bot_tbs:
    syzygy:
      enabled: false
      paths:
        - "engines/syzygy"
      max_pieces: 7
      move_quality: "best"
    gaviota:
      enabled: false
      paths:
        - "engines/gaviota"
      max_pieces: 5
      min_dtm_to_consider_as_wdl_1: 120
      move_quality: "best"

  homemade_options:

  uci_options:
    Move Overhead: 100
    Hash: 512
    UCI_ShowWDL: true

  silence_stderr: false

abort_time: 30
fake_think_time: false
rate_limiting_delay: 0
move_overhead: 2000
max_takebacks_accepted: 0
quit_after_all_games_finish: false

correspondence:
  move_time: 10
  checkin_period: 5
  disconnect_time: 30
  ponder: false

challenge:
  concurrency: 1
  sort_by: "first"
  preference: "none"
  accept_bot: true
  only_bot: false
  max_increment: 20
  min_increment: 0
  max_base: 1800
  min_base: 0
  max_days: .inf
  min_days: 1
  variants:
    - standard
  time_controls:
    - bullet
    - blitz
    - rapid
    - classical
  modes:
    - casual
    - rated
  recent_bot_challenge_age: 3600
  max_recent_bot_challenges: 1
  bullet_requires_increment: false
  max_simultaneous_games_per_user: 5

greeting:
  hello: "Hi! I'm {me}. Good luck! Type !help for a list of commands I can respond to."
  goodbye: "Good game!"
  hello_spectators: "Hi! I'm {me}. Type !help for a list of commands I can respond to."
  goodbye_spectators: "Thanks for watching!"

pgn_directory: "${RUNTIME_DIR}/pgn"
pgn_file_grouping: "game"

matchmaking:
  allow_matchmaking: false
  allow_during_games: false
  challenge_variant: "random"
  challenge_timeout: 30
  challenge_initial_time:
    - 60
    - 180
  challenge_increment:
    - 1
    - 2
  opponent_rating_difference: 300
  rating_preference: "none"
  challenge_mode: "random"
  challenge_filter: none
  include_challenge_block_list: false
EOF_CONFIG
fi

if [[ "${TOKEN}" == "PUT_LICHESS_BOT_TOKEN_HERE" ]]; then
    echo "==> Edit ${CONFIG_FILE} and set token before running lichess-bot."
fi

echo "==> Validating generated config"
"${BOT_DIR}/venv/bin/python" - "${CONFIG_FILE}" "${BOT_DIR}" <<'PY'
import sys

config_path = sys.argv[1]
bot_dir = sys.argv[2]
sys.path.insert(0, bot_dir)

from lib.config import load_config

load_config(config_path)
PY

echo "==> Validating direct UCI engine startup"
uci_output="$(
    printf 'uci\nsetoption name Move Overhead value 100\nsetoption name Hash value 512\nsetoption name UCI_ShowWDL value true\nisready\nquit\n' \
        | "${BUILD_DIR}/chess-engine"
)"
if [[ "${uci_output}" != *"uciok"* || "${uci_output}" != *"readyok"* ]]; then
    echo "error: UCI startup validation failed" >&2
    printf '%s\n' "${uci_output}" >&2
    exit 1
fi

cat <<EOF_DONE

Setup complete.

Run lichess-bot with:
  ${BOT_DIR}/venv/bin/python ${BOT_DIR}/lichess-bot.py \\
    --config ${CONFIG_FILE} \\
    --logfile ${RUNTIME_DIR}/logs/lichess-bot.log
EOF_DONE
