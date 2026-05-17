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
        grep -E '^(old_version|new_version|old_sha256|new_sha256|openings_sha256|git_commit|tc|rounds|games|sprt|sprt_mode|concurrency|hash_mb|fastchess_version)=' "${metadata}" || true
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

    if [[ -f "${log}" ]]; then
        echo "### Interpretation"
        echo

        decision="inconclusive"
        reason="no terminal SPRT boundary reached in the available log"

        if grep -q 'SPRT:.*H1.*accepted' "${log}" 2>/dev/null; then
            decision="pass"
            reason="H1 accepted (improvement detected)"
        elif grep -q 'SPRT:.*H0.*accepted' "${log}" 2>/dev/null; then
            decision="fail"
            reason="H0 accepted (no improvement)"
        elif grep -q 'SPRT:.*completed' "${log}" 2>/dev/null; then
            reason="SPRT completed but no clear H0/H1 terminal line"
        elif grep -q 'The connection is sluggish\|timed\?[ -]out\|time.*out' "${log}" 2>/dev/null; then
            reason="timeout or connection issue detected"
        fi

        echo "Decision: **${decision}**"
        echo
        echo "Reason: ${reason}"
        echo
    fi
} > "${summary}"

cat "${summary}"
