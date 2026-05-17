#!/bin/bash
# Verify that the current bench signature matches the committed reference.
#
# Usage:
#   VERIFY_BENCH_SIGNATURE=1 ./scripts/verify-bench-signature.sh
#   VERIFY_BENCH_SIGNATURE=1 ./scripts/verify-bench-signature.sh --strict
#
# Set VERIFY_BENCH_SIGNATURE=1 to enable the check.  When unset the script
# skips with exit 0 so it can be registered in CTest without breaking the
# default test suite on intentional signature changes.
#
# BENCH_BIN may be set to point at a specific bench_engine binary.
# Default: $PROJECT_DIR/build/bench_engine
#
# This script compares the deterministic fields of bench output:
#   - Per-position: fen, bestmove, score, depth, nodes
#   - Total: nodes, signature
# TimeMs and NPS are excluded because they vary across platforms and runs.
#
# Exit codes:
#   0 - signature matches reference, or check skipped
#   1 - signature mismatch
#   2 - current bench JSON is malformed
#   3 - reference file not found
#   4 - bench_engine binary not found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REFERENCE="$PROJECT_DIR/test/fixtures/bench-reference.json"
BENCH_BIN="${BENCH_BIN:-$PROJECT_DIR/build/bench_engine}"

STRICT=false
if [ "${1:-}" = "--strict" ]; then
    STRICT=true
fi

if [ "${VERIFY_BENCH_SIGNATURE:-}" != "1" ]; then
    echo "bench signature: SKIP (set VERIFY_BENCH_SIGNATURE=1 to enable)"
    exit 0
fi

if [ ! -f "$REFERENCE" ]; then
    echo "ERROR: reference file not found: $REFERENCE"
    exit 3
fi

if [ ! -x "$BENCH_BIN" ]; then
    echo "ERROR: bench_engine binary not found at: $BENCH_BIN"
    echo "Build it first: cmake --build build --target bench_engine"
    exit 4
fi

CURRENT_JSON=$("$BENCH_BIN" bench --json 2>/dev/null)
if [ -z "$CURRENT_JSON" ]; then
    echo "ERROR: bench_engine produced no output or malformed JSON"
    exit 2
fi

# Extract signature from current output using Python for robustness
CURRENT_SIG=$(echo "$CURRENT_JSON" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d['total']['signature'])" 2>/dev/null)
REF_SIG=$(python3 -c "import json; d=json.load(open('$REFERENCE')); print(d['total']['signature'])" 2>/dev/null)

if [ -z "$CURRENT_SIG" ]; then
    echo "ERROR: could not extract signature from current bench output"
    exit 2
fi

if [ -z "$REF_SIG" ]; then
    echo "ERROR: could not extract signature from reference file"
    exit 2
fi

echo "  expected signature: $REF_SIG"
echo "  actual signature:   $CURRENT_SIG"

if [ "$CURRENT_SIG" != "$REF_SIG" ]; then
    echo ""
    echo "SIGNATURE MISMATCH"
    echo ""
    echo "If this change is intentional, update the reference file with:"
    echo "  ./build/bench_engine bench --json 2>/dev/null | python3 -c \"import json,sys; d=json.load(sys.stdin); [p.pop('timeMs',None) for p in d['positions']]; d['total'].pop('timeMs',None); d['total'].pop('nps',None); print(json.dumps(d,indent=2))\" > test/fixtures/bench-reference.json"
    exit 1
fi

if $STRICT; then
    CURRENT_STRIPPED=$(echo "$CURRENT_JSON" | python3 -c "
import json, sys
data = json.load(sys.stdin)
for p in data['positions']:
    p.pop('timeMs', None)
data['total'].pop('timeMs', None)
data['total'].pop('nps', None)
print(json.dumps(data, indent=2, sort_keys=True))
" 2>/dev/null)

    REF_STRIPPED=$(python3 -c "
import json
with open('$REFERENCE') as f:
    data = json.load(f)
print(json.dumps(data, indent=2, sort_keys=True))
" 2>/dev/null)

    if [ "$CURRENT_STRIPPED" != "$REF_STRIPPED" ]; then
        echo ""
        echo "STRICT MISMATCH: per-position data differs from reference"
        diff <(echo "$REF_STRIPPED") <(echo "$CURRENT_STRIPPED") || true
        exit 1
    fi
    echo "  per-position data: match"
fi

echo "  bench signature: OK (match)"
exit 0
