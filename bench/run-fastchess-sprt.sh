#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

build_engine="${BUILD_ENGINE:-${repo_root}/build/chess-engine}"
new_engine="${NEW_ENGINE:-${script_dir}/chess-engine-new}"
old_engine="${OLD_ENGINE:-${script_dir}/chess-engine-old}"
openings="${OPENINGS:-${repo_root}/books/8moves_v3.pgn}"

tc="${TC:-10+0.1}"
rounds="${ROUNDS:-100000}"
games="${GAMES:-2}"
sprt="${SPRT:-elo0=0 elo1=5 alpha=0.05 beta=0.05}"
draw="${DRAW:-movenumber=34 movecount=8 score=20}"
resign="${RESIGN:-movecount=3 score=600}"
pgnout="${PGNOUT:-${script_dir}/latest-vs-old.pgn}"
configout="${CONFIGOUT:-${script_dir}/latest-vs-old-config.json}"
startup_ms="${STARTUP_MS:-60000}"
ucinewgame_ms="${UCINEWGAME_MS:-60000}"
ping_ms="${PING_MS:-60000}"

if command -v nproc >/dev/null 2>&1; then
    default_concurrency="$(nproc)"
else
    default_concurrency="1"
fi
concurrency="${CONCURRENCY:-${default_concurrency}}"

if ! command -v fastchess >/dev/null 2>&1; then
    echo "error: fastchess was not found in PATH" >&2
    exit 1
fi

if [[ ! -x "${build_engine}" ]]; then
    echo "error: ${build_engine} is missing or not executable; build the project first" >&2
    exit 1
fi

if [[ ! -x "${old_engine}" ]]; then
    echo "error: ${old_engine} is missing or not executable" >&2
    exit 1
fi

# Platform check: warn if the baseline binary was built for a different OS
case "$(uname -s)" in
    Darwin)
        if file "${old_engine}" 2>/dev/null | grep -qi 'elf'; then
            echo "error: ${old_engine} is a Linux ELF binary — it will not run on macOS." >&2
            echo "       Build a macOS baseline with OLD_ENGINE=/path/to/mac-baseline or rebuild from a tagged commit." >&2
            exit 1
        fi
        ;;
    Linux)
        if file "${old_engine}" 2>/dev/null | grep -qi 'mach-o'; then
            echo "error: ${old_engine} is a macOS Mach-O binary — it will not run on Linux." >&2
            echo "       Build a Linux baseline with OLD_ENGINE=/path/to/linux-baseline or rebuild from a tagged commit." >&2
            exit 1
        fi
        ;;
esac

cp "${build_engine}" "${new_engine}"
chmod +x "${new_engine}"

cmd=(
    fastchess
    -engine "cmd=${new_engine}" "name=new"
    -engine "cmd=${old_engine}" "name=old"
    -each "tc=${tc}" "option.Hash=64"
    -rounds "${rounds}"
    -games "${games}"
    -sprt ${sprt}
    -draw ${draw}
    -resign ${resign}
    -concurrency "${concurrency}"
    -startup-ms "${startup_ms}"
    -ucinewgame-ms "${ucinewgame_ms}"
    -ping-ms "${ping_ms}"
    -config "outname=${configout}"
    -pgnout "file=${pgnout}" "notation=san" "nodes=true" "append=false"
)

if [[ -f "${openings}" ]]; then
    cmd+=(-openings "file=${openings}" "format=pgn" "order=random")
fi

cmd+=("$@")

printf 'Copied %s -> %s\n' "${build_engine}" "${new_engine}"
printf 'Running:'
printf ' %q' "${cmd[@]}"
printf '\n'

exec "${cmd[@]}"
