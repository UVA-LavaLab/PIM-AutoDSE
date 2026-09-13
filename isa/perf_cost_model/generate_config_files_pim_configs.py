#!/usr/bin/env python3
"""
Generate the cost-model CSVs for every (PIM config, VF) pair the benchmarks use.

Configs are the .cfg files in benchmarks/configs/. VFs and the per-config
benchmark subsets (e.g. Aquabolt) are read from benchmarks/Makefile, so this
produces exactly the CSVs `make` would otherwise generate on demand:

    isa/perf_cost_model/perf_logs/pim_perf_results_config_<CONFIG>_vf<VF>.csv

Each pair is handled by benchmarks/common/ensure_cost_model.py, which reuses an
existing CSV and locks against concurrent generation of the same one.

Usage:
    python3 generate_config_files_pim_configs.py --dry-run
    python3 generate_config_files_pim_configs.py --jobs 4
    python3 generate_config_files_pim_configs.py --config PIMeval_Bank_LPDDR --vf 4096
"""

import argparse
import concurrent.futures
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BENCH_ROOT = REPO_ROOT / "benchmarks"
CONFIG_DIR = BENCH_ROOT / "configs"
MAKEFILE = BENCH_ROOT / "Makefile"
ENSURE = BENCH_ROOT / "common" / "ensure_cost_model.py"
OUT_DIR = REPO_ROOT / "isa" / "perf_cost_model" / "perf_logs"


def read_makefile():
    """Return ({benchmark: VF}, {config: [benchmarks]}) from benchmarks/Makefile."""
    # Join continuation lines so multi-line assignments parse as one.
    text = MAKEFILE.read_text().replace("\\\n", " ")
    vfs = {m.group(1): int(m.group(2))
           for m in re.finditer(r"^VF_(\w+)\s*:?=\s*(\d+)\s*$", text, re.M)}
    subsets = {m.group(1): m.group(2).split()
               for m in re.finditer(r"^BENCHMARKS_(\w+)\s*:?=\s*(.+)$", text, re.M)}
    return vfs, subsets


def cost_model_pairs(configs=None, only_vfs=None):
    vfs, subsets = read_makefile()
    pairs = []
    for cfg in sorted(CONFIG_DIR.glob("*.cfg")):
        if configs and cfg.stem not in configs:
            continue
        benches = subsets.get(cfg.stem, vfs.keys())
        for vf in sorted({vfs[b] for b in benches}):
            if only_vfs and vf not in only_vfs:
                continue
            pairs.append((cfg, vf))
    return pairs


def generate(pair):
    cfg, vf = pair
    cmd = [sys.executable, str(ENSURE), "--config", str(cfg), "--vf", str(vf),
           "--out-dir", str(OUT_DIR)]
    return pair, subprocess.call(cmd)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", action="append",
                    help="config name from benchmarks/configs (repeatable; default: all)")
    ap.add_argument("--vf", action="append", type=int,
                    help="restrict to this VF (repeatable; default: every benchmark VF)")
    ap.add_argument("--jobs", type=int, default=2,
                    help="pairs generated in parallel (default: 2)")
    ap.add_argument("--dry-run", action="store_true",
                    help="list the pairs and whether each CSV already exists")
    args = ap.parse_args(argv)

    pairs = cost_model_pairs(args.config, args.vf)
    if not pairs:
        print("error: no (config, VF) pairs selected", file=sys.stderr)
        return 2

    if args.dry_run:
        for cfg, vf in pairs:
            csv = OUT_DIR / f"pim_perf_results_config_{cfg.stem}_vf{vf}.csv"
            print(f"{'exists ' if csv.exists() else 'missing'}  {cfg.stem:<26} VF={vf}")
        print(f"{len(pairs)} cost models")
        return 0

    failed = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for (cfg, vf), rc in pool.map(generate, pairs):
            if rc != 0:
                failed.append(f"{cfg.stem} VF={vf}")

    for f in failed:
        print(f"failed: {f}", file=sys.stderr)
    print(f"{len(pairs) - len(failed)}/{len(pairs)} cost models ready in {OUT_DIR}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
