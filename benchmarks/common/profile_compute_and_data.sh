#!/usr/bin/env bash
#
# Run one benchmark's two profiling binaries and collect their logs.
#
# Each (benchmark, mode) pair produces two runs:
#   *_compute_log         full run; PIM Command Stats carry the per-command cost
#   *_data_movement_log   compute suppressed; isolates host<->device transfer
#
# The stats parser consumes both and sums them for the end-to-end number.
#
# Environment:
#   PIM_CONFIG   absolute path to the .cfg selecting the hardware target
#   LOG_DIR      where to write logs      (default: ./logs/<config>)
#   MODE         fused | unfused          (default: fused)
#
set -uo pipefail

BENCHMARK="${1:?usage: profile_compute_and_data.sh <benchmark>}"
MODE="${MODE:-fused}"
LOG_DIR="${LOG_DIR:-./logs}"
PIM_CONFIG="${PIM_CONFIG:?PIM_CONFIG must be set}"

mkdir -p "$LOG_DIR"

COMPUTE_BIN="./${BENCHMARK}/bin/${BENCHMARK}_compute_run.out"
DATAMOV_BIN="./${BENCHMARK}/bin/${BENCHMARK}_data_movement_run.out"

COMPUTE_LOG="${LOG_DIR}/${BENCHMARK}_${MODE}_compute_log"
DATAMOV_LOG="${LOG_DIR}/${BENCHMARK}_${MODE}_data_movement_log"
HEADER_SRC="${BENCHMARK}_pim_header.h"
HEADER_DST="${LOG_DIR}/${BENCHMARK}_${MODE}_pim_header.h"

status=0

run_one() {
    local label="$1" bin="$2" log="$3"

    if [ ! -x "$bin" ]; then
        echo "[error] $label binary missing: $bin" >&2
        return 1
    fi

    echo "  [$BENCHMARK/$MODE] $label"
    # The simulator writes its stats block to stdout and diagnostics to stderr;
    # both are captured so the parser sees the whole picture.
    if ! PIM_CONFIG="$PIM_CONFIG" "$bin" > "$log" 2>&1; then
        echo "[error] $label run failed for $BENCHMARK ($MODE); see $log" >&2
        return 1
    fi

    # A run can exit 0 yet produce no stats block (e.g. the device failed to
    # initialise). Catch that here rather than letting the parser emit an
    # empty row.
    if ! grep -q 'PIM Command Stats:' "$log"; then
        echo "[error] $label run produced no stats block for $BENCHMARK ($MODE)" >&2
        return 1
    fi
    return 0
}

run_one "compute      " "$COMPUTE_BIN" "$COMPUTE_LOG" || status=1
run_one "data movement" "$DATAMOV_BIN" "$DATAMOV_LOG" || status=1

if [ -f "$HEADER_SRC" ]; then
    cp "$HEADER_SRC" "$HEADER_DST"
fi

exit $status
