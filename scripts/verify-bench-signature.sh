#!/bin/bash
# Verify that the current bench signature matches the committed reference.
#
# Usage:
#   ./scripts/verify-bench-signature.sh            # compare signature only (exit 0 = match)
#   ./scripts/verify-bench-signature.sh --strict   # also compare per-position bestmove and nodes
#
# This script compares the deterministic fields of bench output:
#   - Per-position: fen, bestmove, score, depth, nodes
#   - Total: nodes, signature
# TimeMs and NPS are excluded because they vary across platforms and runs.
#
# Exit codes:
#   0 - signature matches reference
#   1 - signature mismatch
#   2 - current bench JSON is malformed
#   3 - reference file not found
#   4 - bench_engine binary not found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REFERENCE="$PROJECT_DIR/test/fixtures/bench-reference.json"
BENCH_BIN="$PROJECT_DIR/build/bench_engine"

STRICT=false
if [ "${1:-}" = "--strict" ]; then
    STRICT=true
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

# Extract signature from current output
CURRENT_SIG=$(echo "$CURRENT_JSON" | grep '"signature"' | grep -oE '[0-9a-f]{16}')
REF_SIG=$(grep '"signature"' "$REFERENCE" | grep -oE '[0-9a-f]{16}')

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
    echo "  ./build/bench_engine bench --json 2>/dev/null | sed 's/\"timeMs\": [0-9]*,[[:space:]]*//g' | sed 's/[[:space:]]*\"nps\": [0-9]*,[[:space:]]*//g' | sed 's/[[:space:]]*\"timeMs\": [0-9]*//g' | sed 's/,[[:space:]]*\"nps\": [0-9]*//g' > test/fixtures/bench-reference.json"
    exit 1
fi

if $STRICT; then
    # Compare per-position bestmove and nodes (strip timeMs/nps from current JSON)
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
