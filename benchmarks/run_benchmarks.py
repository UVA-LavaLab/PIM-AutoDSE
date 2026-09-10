#!/usr/bin/env python3
"""
Run PIM-AutoDSE benchmarks across hardware targets and fusion modes, then
collect the simulator's stats into a CSV.

One benchmark tree is retargeted by config file rather than copied per target,
so a sweep is the cross product of:

    benchmarks  x  configs (configs/*.cfg)  x  modes (fused, unfused)

Examples:
    python3 run_benchmarks.py --list
    python3 run_benchmarks.py --benchmark axpy
    python3 run_benchmarks.py --benchmark axpy --config PIMeval_Bank_Rank20
    python3 run_benchmarks.py --all --csv results/sweep.csv
    python3 run_benchmarks.py --all --dry-run

Requires `source env.sh` first (HALIDE_DISTRIB, LLVM_ROOT, PIM_*).

Note: a full sweep compiles and simulates every combination and can take
hours. Use --dry-run first to see the plan.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

BENCH_ROOT = Path(__file__).resolve().parent
CONFIG_DIR = BENCH_ROOT / "configs"
LOG_ROOT = BENCH_ROOT / "logs"

sys.path.insert(0, str(BENCH_ROOT / "stats"))

MODES = ("fused", "unfused")


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def available_configs() -> list[str]:
    return sorted(p.stem for p in CONFIG_DIR.glob("*.cfg"))


def available_benchmarks() -> list[str]:
    """Read the benchmark list out of the Makefile so there is one source of truth."""
    makefile = BENCH_ROOT / "Makefile"
    names: list[str] = []
    collecting = False

    for line in makefile.read_text().splitlines():
        if line.startswith("benchmarks ="):
            collecting = True
            line = line.split("=", 1)[1]
        elif not collecting:
            continue

        continues = line.rstrip().endswith("\\")
        names.extend(line.replace("\\", "").split())
        if not continues:
            break

    # Keep only those that actually have sources checked in.
    return [n for n in names if (BENCH_ROOT / n / "src").is_dir()]


# ---------------------------------------------------------------------------
# Running
# ---------------------------------------------------------------------------

@dataclass
class RunResult:
    benchmark: str
    config: str
    mode: str
    ok: bool
    seconds: float
    message: str = ""


def check_env() -> list[str]:
    """Return a list of problems with the current environment."""
    problems = []
    if not os.environ.get("HALIDE_DISTRIB"):
        problems.append("HALIDE_DISTRIB is not set - run: source ../env.sh")
    if not (os.environ.get("LLVM_ROOT") or os.environ.get("LLVM_DIS_ROOT")):
        problems.append("LLVM_ROOT/LLVM_DIS_ROOT is not set - run: source ../env.sh")
    if shutil.which("make") is None:
        problems.append("make not found on PATH")
    return problems


def run_one(benchmark: str, config: str, mode: str,
            *, jobs: int, clean: bool, dry_run: bool,
            timeout: int | None) -> RunResult:
    fused = "1" if mode == "fused" else "0"
    cmd = ["make", benchmark, f"CONFIG={config}", f"FUSED={fused}", f"-j{jobs}"]

    label = f"{benchmark} [{config}/{mode}]"
    if dry_run:
        print(f"  would run: {' '.join(cmd)}")
        return RunResult(benchmark, config, mode, True, 0.0, "dry-run")

    if clean:
        subprocess.run(["make", "clean"], cwd=BENCH_ROOT,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=False)

    print(f"==> {label}")
    started = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=BENCH_ROOT, timeout=timeout,
                              capture_output=True, text=True)
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - started
        print(f"    TIMEOUT after {elapsed:.0f}s")
        return RunResult(benchmark, config, mode, False, elapsed, "timeout")

    elapsed = time.monotonic() - started

    if proc.returncode != 0:
        # Surface the tail of the build/run output - a silent failure here
        # would show up later as a missing CSV row with no explanation.
        tail = (proc.stderr or proc.stdout or "").strip().splitlines()[-15:]
        print(f"    FAILED (exit {proc.returncode}, {elapsed:.0f}s)")
        for line in tail:
            print(f"      | {line}")
        return RunResult(benchmark, config, mode, False, elapsed,
                         f"exit {proc.returncode}")

    print(f"    ok ({elapsed:.0f}s)")
    return RunResult(benchmark, config, mode, True, elapsed)


# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    ap.add_argument("--list", action="store_true",
                    help="list available benchmarks and configs, then exit")
    ap.add_argument("--benchmark", "-b", action="append", metavar="NAME",
                    help="benchmark to run (repeatable; default: all)")
    ap.add_argument("--config", "-c", action="append", metavar="NAME",
                    help="hardware config to target (repeatable; default: all)")
    ap.add_argument("--mode", "-m", action="append", choices=MODES,
                    help="fusion mode (repeatable; default: both)")
    ap.add_argument("--all", action="store_true",
                    help="full sweep: every benchmark x config x mode")
    ap.add_argument("--csv", type=Path, metavar="PATH",
                    help="parse logs into this CSV when the sweep finishes")
    ap.add_argument("--commands-csv", type=Path, metavar="PATH",
                    help="also write the per-command breakdown here")
    ap.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 4,
                    help="parallel make jobs per benchmark (default: %(default)s)")
    ap.add_argument("--clean", action="store_true",
                    help="make clean before each run (slower, but avoids stale codegen)")
    ap.add_argument("--timeout", type=int, metavar="SEC",
                    help="per-run timeout in seconds")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan without building or running anything")
    ap.add_argument("--keep-going", action="store_true",
                    help="continue after a failed run (default: stop)")
    args = ap.parse_args(argv)

    all_benchmarks = available_benchmarks()
    all_configs = available_configs()

    if args.list:
        print("Benchmarks:")
        for name in all_benchmarks:
            print(f"  {name}")
        print("\nConfigs:")
        for name in all_configs:
            print(f"  {name}")
        print("\nModes:")
        for name in MODES:
            print(f"  {name}")
        return 0

    benchmarks = args.benchmark or (all_benchmarks if args.all else None)
    if not benchmarks:
        ap.error("specify --benchmark, or --all for a full sweep "
                 "(--list shows what is available)")

    configs = args.config or all_configs
    modes = tuple(args.mode) if args.mode else MODES

    unknown_b = [b for b in benchmarks if b not in all_benchmarks]
    unknown_c = [c for c in configs if c not in all_configs]
    if unknown_b:
        ap.error(f"unknown benchmark(s): {', '.join(unknown_b)}")
    if unknown_c:
        ap.error(f"unknown config(s): {', '.join(unknown_c)}")

    if not args.dry_run:
        problems = check_env()
        if problems:
            for p in problems:
                print(f"error: {p}", file=sys.stderr)
            return 2

    total = len(benchmarks) * len(configs) * len(modes)
    print(f"Plan: {len(benchmarks)} benchmark(s) x {len(configs)} config(s) "
          f"x {len(modes)} mode(s) = {total} run(s)\n")

    results: list[RunResult] = []
    for config in configs:
        for benchmark in benchmarks:
            for mode in modes:
                result = run_one(benchmark, config, mode,
                                 jobs=args.jobs, clean=args.clean,
                                 dry_run=args.dry_run, timeout=args.timeout)
                results.append(result)
                if not result.ok and not args.keep_going:
                    print("\nstopping after first failure "
                          "(use --keep-going to continue)", file=sys.stderr)
                    _summarize(results)
                    return 1

    _summarize(results)

    if args.dry_run:
        return 0

    if args.csv or args.commands_csv:
        _collect(args.csv, args.commands_csv)

    return 0 if all(r.ok for r in results) else 1


def _summarize(results: list[RunResult]) -> None:
    ok = [r for r in results if r.ok]
    bad = [r for r in results if not r.ok]
    elapsed = sum(r.seconds for r in results)

    print(f"\n{'-' * 60}")
    print(f"{len(ok)}/{len(results)} run(s) succeeded in {elapsed / 60:.1f} min")
    if bad:
        print("\nFailed:")
        for r in bad:
            print(f"  {r.benchmark} [{r.config}/{r.mode}]: {r.message}")


def _collect(csv_path: Path | None, commands_csv: Path | None) -> None:
    """Parse every log under logs/ into CSV."""
    try:
        import parse_logs
    except ImportError as exc:
        print(f"error: cannot import the stats parser: {exc}", file=sys.stderr)
        return

    if not LOG_ROOT.is_dir():
        print(f"error: no logs directory at {LOG_ROOT}", file=sys.stderr)
        return

    argv = [str(LOG_ROOT), "--recursive"]
    if csv_path:
        argv += ["--csv", str(csv_path)]
    if commands_csv:
        argv += ["--commands-csv", str(commands_csv)]

    print(f"\nCollecting stats from {LOG_ROOT} ...")
    parse_logs.main(argv)


if __name__ == "__main__":
    sys.exit(main())
