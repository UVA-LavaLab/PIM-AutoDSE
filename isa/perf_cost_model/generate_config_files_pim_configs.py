#!/usr/bin/env python3
"""
Generate the cost-model CSVs for every (PIM config, VF) pair the benchmarks use.

Configs are the .cfg files in benchmarks/configs/. VFs and the per-config
benchmark subsets (e.g. Aquabolt) are read from benchmarks/Makefile, so this
produces exactly the CSVs `make` would otherwise generate on demand:

    isa/perf_cost_model/perf_logs/pim_perf_results_config_<CONFIG>_vf<VF>.csv

Each pair is handled by benchmarks/common/ensure_cost_model.py, which reuses an
existing CSV and locks against concurrent generation of the same one, so an
interrupted run can simply be restarted.

Pairs run smallest VF first, so a long sweep produces usable CSVs early. Large
VFs are slow: each pair compiles the full lowering headers twice and runs every
operation at that VF, and the simulator logs for VF >= 16777216 can reach tens
of GB per pair while it runs. Limit the sweep with --max-vf if disk is tight.

Each pair's output goes to perf_logs/.logs/<CONFIG>_vf<VF>.log.

Usage (from the repository root, after `source env.sh`):
    python3 isa/perf_cost_model/generate_config_files_pim_configs.py --dry-run
    python3 isa/perf_cost_model/generate_config_files_pim_configs.py --jobs 4
    python3 isa/perf_cost_model/generate_config_files_pim_configs.py --jobs 4 --max-vf 4194304
    python3 isa/perf_cost_model/generate_config_files_pim_configs.py --config PIMeval_Bank_LPDDR --vf 4096
"""

import argparse
import concurrent.futures
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BENCH_ROOT = REPO_ROOT / "benchmarks"
CONFIG_DIR = BENCH_ROOT / "configs"
MAKEFILE = BENCH_ROOT / "Makefile"
ENSURE = BENCH_ROOT / "common" / "ensure_cost_model.py"
OUT_DIR = REPO_ROOT / "isa" / "perf_cost_model" / "perf_logs"
LOG_DIR = OUT_DIR / ".logs"

_print_lock = threading.Lock()


def log(msg):
    with _print_lock:
        print(f"{time.strftime('%H:%M:%S')} {msg}", flush=True)


def rel(path):
    """Path relative to the repo root for display, or absolute if outside it."""
    try:
        return path.relative_to(REPO_ROOT)
    except ValueError:
        return path


def fmt_duration(seconds):
    seconds = int(seconds)
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h}h{m:02d}m{s:02d}s" if h else f"{m}m{s:02d}s"


def read_makefile():
    """Return ({benchmark: VF}, {config: [benchmarks]}) from benchmarks/Makefile."""
    # Join continuation lines so multi-line assignments parse as one.
    text = MAKEFILE.read_text().replace("\\\n", " ")
    vfs = {m.group(1): int(m.group(2))
           for m in re.finditer(r"^VF_(\w+)\s*:?=\s*(\d+)\s*$", text, re.M)}
    subsets = {m.group(1): m.group(2).split()
               for m in re.finditer(r"^BENCHMARKS_(\w+)\s*:?=\s*(.+)$", text, re.M)}
    return vfs, subsets


def csv_path(cfg, vf):
    return OUT_DIR / f"pim_perf_results_config_{cfg.stem}_vf{vf}.csv"


def cost_model_pairs(configs=None, exclude=None, only_vfs=None, max_vf=None):
    vfs, subsets = read_makefile()
    pairs = []
    for cfg in sorted(CONFIG_DIR.glob("*.cfg")):
        if configs and cfg.stem not in configs:
            continue
        if exclude and cfg.stem in exclude:
            continue
        benches = subsets.get(cfg.stem, vfs.keys())
        for vf in sorted({vfs[b] for b in benches}):
            if only_vfs and vf not in only_vfs:
                continue
            if max_vf and vf > max_vf:
                continue
            pairs.append((cfg, vf))
    # Smallest VF first; configs in name order within a VF.
    pairs.sort(key=lambda p: (p[1], p[0].stem))
    return pairs


def generate(pair):
    """Generate one CSV. Never raises: any failure is reported and returned as a
    nonzero code so the remaining pairs keep running."""
    cfg, vf = pair
    name = f"{cfg.stem} VF={vf}"
    start = time.time()
    try:
        if csv_path(cfg, vf).exists():
            log(f"exists    {name}")
            return pair, 0, 0.0
        log_file = LOG_DIR / f"{cfg.stem}_vf{vf}.log"
        log(f"start     {name}  (log: {rel(log_file)})")
        cmd = [sys.executable, "-u", str(ENSURE), "--config", str(cfg), "--vf", str(vf),
               "--out-dir", str(OUT_DIR)]
        with open(log_file, "w") as out:
            rc = subprocess.call(cmd, stdout=out, stderr=subprocess.STDOUT)
        elapsed = time.time() - start
        if rc == 0 and not csv_path(cfg, vf).exists():
            rc = 1
            reason = f"exited 0 but wrote no CSV, see {rel(log_file)}"
        else:
            reason = f"exit {rc}, see {rel(log_file)}"
        if rc == 0:
            log(f"done      {name}  ({fmt_duration(elapsed)})")
        else:
            log(f"FAILED    {name}  ({fmt_duration(elapsed)}, {reason})")
        return pair, rc, elapsed
    except Exception as exc:
        elapsed = time.time() - start
        log(f"FAILED    {name}  ({fmt_duration(elapsed)}, {type(exc).__name__}: {exc})")
        return pair, -1, elapsed


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", action="append",
                    help="config name from benchmarks/configs (repeatable; default: all)")
    ap.add_argument("--exclude-config", action="append",
                    help="skip this config (repeatable)")
    ap.add_argument("--vf", action="append", type=int,
                    help="restrict to this VF (repeatable; default: every benchmark VF)")
    ap.add_argument("--max-vf", type=int,
                    help="skip VFs larger than this")
    ap.add_argument("--jobs", type=int, default=4,
                    help="pairs generated in parallel (default: 4)")
    ap.add_argument("--dry-run", action="store_true",
                    help="list the pairs and whether each CSV already exists")
    args = ap.parse_args(argv)

    pairs = cost_model_pairs(args.config, args.exclude_config, args.vf, args.max_vf)
    if not pairs:
        print("error: no (config, VF) pairs selected", file=sys.stderr)
        return 2

    missing = [p for p in pairs if not csv_path(*p).exists()]
    if args.dry_run:
        for cfg, vf in pairs:
            state = "exists " if csv_path(cfg, vf).exists() else "missing"
            print(f"{state}  {cfg.stem:<26} VF={vf}")
        print(f"{len(pairs)} cost models, {len(missing)} to generate")
        return 0

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    log(f"{len(pairs)} cost models, {len(missing)} to generate, {args.jobs} in parallel")

    start = time.time()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for result in pool.map(generate, pairs):
            results.append(result)

    failed = [(cfg, vf) for (cfg, vf), rc, _ in results if rc != 0]
    print()
    print(f"{'config':<26} {'VF':>9}  {'status':<8} time")
    for (cfg, vf), rc, elapsed in results:
        status = "FAILED" if rc != 0 else ("reused" if elapsed == 0.0 else "ok")
        print(f"{cfg.stem:<26} {vf:>9}  {status:<8} {fmt_duration(elapsed) if elapsed else '-'}")
    print()
    print(f"{len(pairs) - len(failed)}/{len(pairs)} cost models ready in "
          f"{rel(OUT_DIR)} (total {fmt_duration(time.time() - start)})")
    if failed:
        print("failed: " + ", ".join(f"{c.stem} VF={v}" for c, v in failed), file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
