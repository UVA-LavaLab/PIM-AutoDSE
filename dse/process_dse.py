#!/usr/bin/env python3
"""
Tabulate a DSE sweep's simulator logs into a CSV.

For each configuration in a log directory this pairs the compute log with its
data-movement log, extracts the PIM stats, and writes one row per config.

    python3 process_dse.py --log-dir dse_logs_perf_opt --benchmark histogram
    python3 process_dse.py --log-dir dse_logs --benchmark gemv_v1 -o out.csv

Two bugs in the original version are fixed here:

  * It derived the second input as
        file.replace("compute_log", "data_log")
    while the benchmark driver writes `*_data_movement_log`. The two naming
    conventions disagreed, so the pair never resolved.
  * The whole loop body ran under a bare `except: continue`, so that mismatch
    - and any other failure - silently produced a CSV with missing rows
    instead of an error. Failures are now reported.

`--data-suffix` covers log directories written by either convention.
"""

from __future__ import annotations

import argparse
import glob
import os
import sys

import pandas as pd

from PIM_TUNER_UTILS import get_full_pim_result_stats_from_pim_log_files

COMPUTE_SUFFIX = "_compute_log"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log-dir", required=True,
                    help="directory holding the sweep's *_compute_log files")
    ap.add_argument("--benchmark", required=True,
                    help="benchmark name to filter on (e.g. histogram)")
    ap.add_argument("-o", "--output", default=None,
                    help="output CSV (default: <benchmark>_dse.csv)")
    ap.add_argument("--data-suffix", default="_data_movement_log",
                    choices=["_data_movement_log", "_data_log"],
                    help="naming convention for the data-movement log "
                         "(default: %(default)s)")
    ap.add_argument("--strict", action="store_true",
                    help="exit non-zero if any configuration fails to parse")
    args = ap.parse_args(argv)

    pattern = os.path.join(args.log_dir, f"*{args.benchmark}*{COMPUTE_SUFFIX}")
    compute_files = sorted(glob.glob(pattern))

    if not compute_files:
        print(f"error: no logs matched {pattern}", file=sys.stderr)
        return 1

    print(f"found {len(compute_files)} configuration(s) for {args.benchmark}")

    stats_by_config: dict[str, dict] = {}
    failures: list[tuple[str, str]] = []

    for compute_file in compute_files:
        config_name = os.path.basename(compute_file)[: -len(COMPUTE_SUFFIX)]
        data_file = compute_file[: -len(COMPUTE_SUFFIX)] + args.data_suffix

        if not os.path.exists(data_file):
            failures.append((config_name, f"missing {os.path.basename(data_file)}"))
            continue

        # Narrow except: a malformed log is reported with its cause rather than
        # being dropped from the table.
        try:
            stats = get_full_pim_result_stats_from_pim_log_files(
                compute_file, data_file)
        except (OSError, ValueError, KeyError, IndexError) as exc:
            failures.append((config_name, f"{type(exc).__name__}: {exc}"))
            continue

        stats_by_config[config_name] = stats

    if not stats_by_config:
        print("error: no configuration parsed successfully", file=sys.stderr)
        for name, why in failures:
            print(f"  {name}: {why}", file=sys.stderr)
        return 1

    df = pd.DataFrame(stats_by_config).transpose()
    output = args.output or f"{args.benchmark}_dse.csv"
    df.to_csv(output, index=True)
    print(f"wrote {len(df)} row(s) -> {output}")

    if failures:
        print(f"\n{len(failures)} configuration(s) failed:", file=sys.stderr)
        for name, why in failures:
            print(f"  {name}: {why}", file=sys.stderr)
        if args.strict:
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
