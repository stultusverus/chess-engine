#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

books_dir="${BOOKS_DIR:-${script_dir}/books}"
vers_dir="${VERS_DIR:-${script_dir}/vers}"
runs_dir="${RUNS_DIR:-${script_dir}/runs}"
openings="${OPENINGS:-${books_dir}/8moves_v3.pgn}"

tc="${TC:-10+0.1}"
rounds="${ROUNDS:-100000}"
games="${GAMES:-2}"
sprt="${SPRT:-elo0=0 elo1=5 alpha=0.05 beta=0.05}"
draw="${DRAW:-movenumber=34 movecount=8 score=20}"
resign="${RESIGN:-movecount=3 score=600}"
startup_ms="${STARTUP_MS:-60000}"
ucinewgame_ms="${UCINEWGAME_MS:-60000}"
ping_ms="${PING_MS:-60000}"
hash_mb="${HASH_MB:-64}"
quiet_console="${QUIET_CONSOLE:-0}"

usage() {
    cat >&2 <<EOF
Usage: $(basename "$0") OLD_VERSION NEW_VERSION [additional fastchess args...]

Expected layout:
  $(basename "$script_dir")/
    books/8moves_v3.pgn
    runs/
    vers/OLD_VERSION
    vers/NEW_VERSION

Examples:
  $(basename "$0") v1.2 v1.3
  TC='5+0.05' CONCURRENCY=12 $(basename "$0") v1.2 v1.3

Environment overrides:
  BOOKS_DIR, VERS_DIR, RUNS_DIR, OPENINGS
  TC, ROUNDS, GAMES, SPRT, DRAW, RESIGN
  STARTUP_MS, UCINEWGAME_MS, PING_MS, HASH_MB, CONCURRENCY, QUIET_CONSOLE
EOF
}

if [[ $# -lt 2 ]]; then
    usage
    exit 2
fi

old_version="$1"
new_version="$2"
shift 2

old_engine="${vers_dir}/${old_version}"
new_engine="${vers_dir}/${new_version}"

safe_old_version="${old_version//[^A-Za-z0-9._-]/_}"
safe_new_version="${new_version//[^A-Za-z0-9._-]/_}"
timestamp="$(date '+%Y%m%d-%H%M%S')"
run_name="${timestamp}-${safe_old_version}-vs-${safe_new_version}"
run_dir="${RUN_DIR:-${runs_dir}/${run_name}}"

pgnout="${PGNOUT:-${run_dir}/games.pgn}"
configout="${CONFIGOUT:-${run_dir}/fastchess-config.json}"
command_log="${COMMAND_LOG:-${run_dir}/command.sh}"
run_log="${RUN_LOG:-${run_dir}/fastchess.log}"
metadata_file="${METADATA_FILE:-${run_dir}/metadata.txt}"

if command -v nproc >/dev/null 2>&1; then
    default_concurrency="$(nproc)"
elif [[ "$(uname -s)" == "Darwin" ]] && command -v sysctl >/dev/null 2>&1; then
    default_concurrency="$(sysctl -n hw.ncpu)"
else
    default_concurrency="1"
fi
concurrency="${CONCURRENCY:-${default_concurrency}}"

if ! command -v fastchess >/dev/null 2>&1; then
    echo "error: fastchess was not found in PATH" >&2
    exit 1
fi

if [[ ! -x "${old_engine}" ]]; then
    echo "error: ${old_engine} is missing or not executable" >&2
    exit 1
fi

if [[ ! -x "${new_engine}" ]]; then
    echo "error: ${new_engine} is missing or not executable" >&2
    exit 1
fi

# Platform check: fail early if an engine binary was built for a different OS.
case "$(uname -s)" in
    Darwin)
        for engine in "${old_engine}" "${new_engine}"; do
            if file "${engine}" 2>/dev/null | grep -qi 'elf'; then
                echo "error: ${engine} is a Linux ELF binary — it will not run on macOS." >&2
                exit 1
            fi
        done
        ;;
    Linux)
        for engine in "${old_engine}" "${new_engine}"; do
            if file "${engine}" 2>/dev/null | grep -qi 'mach-o'; then
                echo "error: ${engine} is a macOS Mach-O binary — it will not run on Linux." >&2
                exit 1
            fi
        done
        ;;
esac

mkdir -p "${run_dir}"

# These options are intentionally split into FastChess key=value tokens.
# They are not shell-evaluated; do not put shell quotes inside the variable values.
read -r -a sprt_args <<< "${sprt}"
read -r -a draw_args <<< "${draw}"
read -r -a resign_args <<< "${resign}"

cmd=(
    fastchess
    -engine "cmd=${new_engine}" "name=${new_version}"
    -engine "cmd=${old_engine}" "name=${old_version}"
    -each "tc=${tc}" "option.Hash=${hash_mb}"
    -rounds "${rounds}"
    -games "${games}"
    -sprt "${sprt_args[@]}"
    -draw "${draw_args[@]}"
    -resign "${resign_args[@]}"
    -concurrency "${concurrency}"
    -startup-ms "${startup_ms}"
    -ucinewgame-ms "${ucinewgame_ms}"
    -ping-ms "${ping_ms}"
    -config "outname=${configout}"
    -pgnout "file=${pgnout}" "notation=san" "nodes=true" "append=false"
)

if [[ -f "${openings}" ]]; then
    cmd+=(-openings "file=${openings}" "format=pgn" "order=random")
else
    echo "warning: openings file not found; continuing without openings: ${openings}" >&2
fi

cmd+=("$@")

{
    printf 'timestamp=%s\n' "${timestamp}"
    printf 'run_dir=%s\n' "${run_dir}"
    printf 'old_version=%s\n' "${old_version}"
    printf 'new_version=%s\n' "${new_version}"
    printf 'old_engine=%s\n' "${old_engine}"
    printf 'new_engine=%s\n' "${new_engine}"
    printf 'openings=%s\n' "${openings}"
    printf 'tc=%s\n' "${tc}"
    printf 'rounds=%s\n' "${rounds}"
    printf 'games=%s\n' "${games}"
    printf 'sprt=%s\n' "${sprt}"
    printf 'draw=%s\n' "${draw}"
    printf 'resign=%s\n' "${resign}"
    printf 'concurrency=%s\n' "${concurrency}"
    printf 'hash_mb=%s\n' "${hash_mb}"
} > "${metadata_file}"

{
    printf '#!/usr/bin/env bash\n'
    printf 'cd %q\n' "${run_dir}"
    printf 'exec'
    printf ' %q' "${cmd[@]}"
    printf '\n'
} > "${command_log}"
chmod +x "${command_log}"

printf 'Run directory: %s\n' "${run_dir}"
printf 'Metadata:      %s\n' "${metadata_file}"
printf 'Command log:   %s\n' "${command_log}"
printf 'FastChess log: %s\n' "${run_log}"
printf 'PGN output:    %s\n' "${pgnout}"
printf 'Config output: %s\n' "${configout}"
printf 'Quiet console: %s\n' "${quiet_console}"
printf 'Running:'
printf ' %q' "${cmd[@]}"
printf '\n'

cd "${run_dir}"
if [[ "${quiet_console}" == "1" ]]; then
    # Keep the full FastChess log, but drop verbose position dumps from the terminal.
    exec > >(tee -a "${run_log}" | grep -Ev '^(Position;|Moves;)') 2>&1
else
    exec > >(tee -a "${run_log}") 2>&1
fi
exec "${cmd[@]}"
