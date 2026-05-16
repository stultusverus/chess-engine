#!/usr/bin/env bash
set -euo pipefail

run_dir="${1:-}"

if [[ -z "${run_dir}" || ! -d "${run_dir}" ]]; then
    echo "usage: $0 RUN_DIR" >&2
    exit 2
fi

log="${run_dir}/fastchess.log"
metadata="${run_dir}/metadata.txt"
summary="${run_dir}/summary.md"

{
    echo "## SPRT summary"
    echo
    echo "**Run directory:** \`${run_dir}\`"
    echo

    if [[ -f "${metadata}" ]]; then
        echo "### Metadata"
        echo
        echo '```text'
        grep -E '^(old_version|new_version|old_sha256|new_sha256|openings_sha256|git_commit|tc|rounds|games|sprt|concurrency|hash_mb|fastchess_version)=' "${metadata}" || true
        echo '```'
        echo
    fi

    if [[ -f "${log}" ]]; then
        echo "### FastChess result tail"
        echo
        echo '```text'
        grep -E '^(Results of|Elo:|LLR:|Games:|Ptnml|Finished|SPRT|Score of|Warning:|error:)' "${log}" | tail -n 80 || tail -n 80 "${log}"
        echo '```'
        echo
    else
        echo "FastChess log not found: ${log}"
    fi
} > "${summary}"

cat "${summary}"
