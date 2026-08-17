#!/usr/bin/env bash
#
# run_all.sh — runs every v2 end-to-end handler test and reports a summary.
#
# Each test_<handler>.sh is self-contained (starts/stops its own server on
# its own port), so this just runs them one after another and aggregates
# pass/fail. Set SERVER_BIN to point at a specific my_redis_server binary;
# otherwise each test probes the usual build output locations.
#
# Usage: ./run_all.sh [test_file...]   (defaults to every test_*.sh here)
#
set -uo pipefail

E2E_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -gt 0 ]]; then
  TEST_FILES=("$@")
else
  TEST_FILES=("$E2E_DIR"/test_*.sh)
fi

TOTAL=0
FAILED=0
FAILED_NAMES=()

for test_file in "${TEST_FILES[@]}"; do
  name="$(basename "$test_file")"
  echo "== $name =="
  TOTAL=$((TOTAL + 1))
  if bash "$test_file"; then
    echo "  PASS"
  else
    echo "  FAIL"
    FAILED=$((FAILED + 1))
    FAILED_NAMES+=("$name")
  fi
  echo
done

echo "===================================="
echo "$((TOTAL - FAILED))/$TOTAL test files passed"
if (( FAILED > 0 )); then
  echo "failed: ${FAILED_NAMES[*]}"
  exit 1
fi
