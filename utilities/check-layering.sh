#!/usr/bin/env sh
# Layering check wrapper (see check-layering.sh).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COUNT=$(find "$ROOT/src/common" \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 grep -l '#include.*\.\./.*game/' 2>/dev/null | wc -l | tr -d ' ')
echo "Common files including game/: $COUNT"
echo "Baseline: count should decrease over time; see docs/foundation-audit-baseline.md"
exit 0
