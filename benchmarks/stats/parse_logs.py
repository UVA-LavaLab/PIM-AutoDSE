#!/usr/bin/env python3
"""
Parse PIMeval simulator logs into structured records.

Each (benchmark, config, mode) run produces two logs:

  *_compute_log          full run; `PIM Command Stats` carries the per-command
                         compute cost
  *_data_movement_log    same run with compute suppressed; its `Data Copy
                         Stats` isolate host<->device transfer cost

The end-to-end cost is compute (from the first) plus data movement (from the
second). Both logs report a `Data Copy Stats` block; taking it only from the
data-movement log avoids double counting.

Usage:
    python3 parse_logs.py logs/PIMeval_Bank_LPDDR --csv results.csv
    python3 parse_logs.py logs --recursive --csv all.csv
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Line patterns
# ---------------------------------------------------------------------------

# "            Number of PIM Cores : 1280"
_PARAM_RE = re.compile(r"^\s*(?P<key>[^:]+?)\s*:\s*(?P<val>.+?)\s*$")

# "                    Host to Device : 524288 bytes"
_COPY_RE = re.compile(
    r"^\s*(?P<key>Host to Device|Device to Host|Device to Device)\s*:\s*"
    r"(?P<bytes>\d+)\s*bytes\s*$"
)

# "  TOTAL --------- : 786432 bytes   0.002861 ms Estimated Runtime   0.006454 mj Estimated Energy"
_COPY_TOTAL_RE = re.compile(
    r"^\s*TOTAL\s*-*\s*:\s*(?P<bytes>\d+)\s*bytes\s+"
    r"(?P<runtime>[-\w.]+)\s*ms\s+Estimated Runtime\s+"
    r"(?P<energy>[-\w.]+)\s*mj\s+Estimated Energy",
    re.IGNORECASE,
)

# "   add.int32.h.fuse :   1   0.000169   0.010359   6.326312 29.4118 50.0000 20.5882"
_CMD_RE = re.compile(
    r"^\s*(?P<name>[A-Za-z][\w.\-]*|TOTAL\s*-*)\s*:\s*"
    r"(?P<cnt>\d+)\s+"
    r"(?P<runtime>[-\w.]+)\s+"
    r"(?P<energy>[-\w.]+)\s+"
    r"(?P<gops_w>[-\w.]+)\s+"
    r"(?P<pct_r>[-\w.]+)\s+"
    r"(?P<pct_w>[-\w.]+)\s+"
    r"(?P<pct_l>[-\w.]+)\s*$"
)

# "PIM-Info: Fusing 6 command groups."
_FUSE_RE = re.compile(r"PIM-Info:\s*Fusing\s+(?P<n>\d+)\s+command groups")

# "PIM-Info: Created PIM device with 1280 cores of 32768 rows and 1024 columns."
_DEVICE_RE = re.compile(
    r"Created PIM device with (?P<cores>\d+) cores of "
    r"(?P<rows>\d+) rows and (?P<cols>\d+) columns"
)

# DRAM command event counters (present in newer builds only).
# "TOTAL ACT: 1234"
_EVENT_RE = re.compile(
    r"^\s*TOTAL\s+(?P<key>ACT|PRE|CAS|Compute)\s*:\s*(?P<val>\d+)\s*$",
    re.IGNORECASE,
)

# Map the human-readable `PIM Params:` labels onto column names.
_PARAM_KEYS = {
    "PIM Device Type Enum": "device_type",
    "PIM Simulation Target": "sim_target",
    "Rank, Bank, Subarray, Row, Col": "geometry",
    "Number of PIM Cores": "n_cores",
    "Number of Rows per Core": "rows_per_core",
    "Number of Cols per Core": "cols_per_core",
    "Typical Rank BW": "rank_bw",
    "Row Read (ns)": "row_read_ns",
    "Row Write (ns)": "row_write_ns",
    "tCCD (ns)": "tccd_ns",
}


def _num(text: str) -> float:
    """Parse a float, mapping the simulator's `-nan`/`nan`/`inf` to NaN.

    A data-movement log's command TOTAL row is all `-nan` because no commands
    ran; that is expected, not an error.
    """
    try:
        value = float(text)
    except ValueError:
        return math.nan
    return value if math.isfinite(value) else math.nan


def _strip_units(text: str) -> str:
    """`25.600000 GB/s` -> `25.600000`"""
    return text.split()[0] if text else text


# ---------------------------------------------------------------------------
# Log model
# ---------------------------------------------------------------------------

@dataclass
class CommandRow:
    name: str
    count: int
    runtime_ms: float
    energy_mj: float
    gops_per_w: float
    pct_r: float
    pct_w: float
    pct_l: float


@dataclass
class LogStats:
    path: str
    params: dict = field(default_factory=dict)
    copy_bytes: dict = field(default_factory=dict)
    copy_total_bytes: int = 0
    copy_runtime_ms: float = 0.0
    copy_energy_mj: float = 0.0
    commands: list = field(default_factory=list)
    cmd_total: CommandRow | None = None
    fused_groups: int = 0
    events: dict = field(default_factory=dict)
    complete: bool = False


def parse_log(path: Path) -> LogStats:
    """Parse a single simulator log."""
    stats = LogStats(path=str(path))
    section = None

    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc

    for line in lines:
        # --- section headers ---
        stripped = line.strip()
        if stripped.startswith("PIM Params:"):
            section = "params"
            continue
        if stripped.startswith("Data Copy Stats:"):
            section = "copy"
            continue
        if stripped.startswith("PIM Command Stats:"):
            section = "cmd"
            stats.complete = True
            continue
        if stripped.startswith("----"):
            section = None
            continue

        # --- section-independent lines ---
        if (m := _FUSE_RE.search(line)) is not None:
            stats.fused_groups = int(m.group("n"))
            continue
        if (m := _DEVICE_RE.search(line)) is not None:
            stats.params.setdefault("n_cores", m.group("cores"))
            stats.params.setdefault("rows_per_core", m.group("rows"))
            stats.params.setdefault("cols_per_core", m.group("cols"))
            continue
        if (m := _EVENT_RE.match(line)) is not None:
            stats.events[f"evt_{m.group('key').lower()}"] = int(m.group("val"))
            continue

        # --- sectioned lines ---
        if section == "params":
            if (m := _PARAM_RE.match(line)) is not None:
                key = _PARAM_KEYS.get(m.group("key").strip())
                if key:
                    stats.params[key] = _strip_units(m.group("val"))
        elif section == "copy":
            if (m := _COPY_TOTAL_RE.match(line)) is not None:
                stats.copy_total_bytes = int(m.group("bytes"))
                stats.copy_runtime_ms = _num(m.group("runtime"))
                stats.copy_energy_mj = _num(m.group("energy"))
            elif (m := _COPY_RE.match(line)) is not None:
                label = m.group("key").lower().replace(" ", "_")
                stats.copy_bytes[label] = int(m.group("bytes"))
        elif section == "cmd":
            if (m := _CMD_RE.match(line)) is not None:
                name = m.group("name").strip()
                if name.upper().startswith("PIM-CMD"):
                    continue  # header row
                row = CommandRow(
                    name="TOTAL" if name.upper().startswith("TOTAL") else name,
                    count=int(m.group("cnt")),
                    runtime_ms=_num(m.group("runtime")),
                    energy_mj=_num(m.group("energy")),
                    gops_per_w=_num(m.group("gops_w")),
                    pct_r=_num(m.group("pct_r")),
                    pct_w=_num(m.group("pct_w")),
                    pct_l=_num(m.group("pct_l")),
                )
                if row.name == "TOTAL":
                    stats.cmd_total = row
                else:
                    stats.commands.append(row)

    return stats


# ---------------------------------------------------------------------------
# Combining the two logs into one record
# ---------------------------------------------------------------------------

def _zero_if_nan(value: float) -> float:
    return 0.0 if math.isnan(value) else value


def combine(compute: LogStats, datamov: LogStats,
            benchmark: str, config: str, mode: str) -> dict:
    """Build one flat CSV row from a compute + data-movement log pair."""
    record: dict = {
        "benchmark": benchmark,
        "config": config,
        "mode": mode,
    }
    record.update(compute.params)

    # Data movement comes from the data-movement log only. The compute log
    # reports the same copy block, so taking both would double count.
    record.update({
        "h2d_bytes": datamov.copy_bytes.get("host_to_device", 0),
        "d2h_bytes": datamov.copy_bytes.get("device_to_host", 0),
        "d2d_bytes": datamov.copy_bytes.get("device_to_device", 0),
        "copy_total_bytes": datamov.copy_total_bytes,
        "data_movement_runtime_ms": datamov.copy_runtime_ms,
        "data_movement_energy_mj": datamov.copy_energy_mj,
    })

    total = compute.cmd_total
    record.update({
        "cmd_count": total.count if total else 0,
        "compute_runtime_ms": total.runtime_ms if total else math.nan,
        "compute_energy_mj": total.energy_mj if total else math.nan,
        "gops_per_w": total.gops_per_w if total else math.nan,
        "pct_r": total.pct_r if total else math.nan,
        "pct_w": total.pct_w if total else math.nan,
        "pct_l": total.pct_l if total else math.nan,
        "fused_groups": compute.fused_groups,
        "n_distinct_cmds": len(compute.commands),
    })

    compute_rt = _zero_if_nan(record["compute_runtime_ms"])
    compute_en = _zero_if_nan(record["compute_energy_mj"])
    record["total_runtime_ms"] = compute_rt + _zero_if_nan(datamov.copy_runtime_ms)
    record["total_energy_mj"] = compute_en + _zero_if_nan(datamov.copy_energy_mj)

    record.update(compute.events)
    return record


def command_rows(compute: LogStats, benchmark: str,
                 config: str, mode: str) -> list[dict]:
    """Long-form per-command rows, for a breakdown CSV."""
    return [
        {
            "benchmark": benchmark, "config": config, "mode": mode,
            "cmd_name": row.name, "cnt": row.count,
            "runtime_ms": row.runtime_ms, "energy_mj": row.energy_mj,
            "gops_per_w": row.gops_per_w,
        }
        for row in compute.commands
    ]


# ---------------------------------------------------------------------------
# Log discovery
# ---------------------------------------------------------------------------

_LOG_SUFFIX = "_compute_log"


def discover(log_dir: Path, recursive: bool = False):
    """Yield (benchmark, config, mode, compute_log, data_movement_log).

    Layout written by the benchmark Makefile:
        logs/<config>/<benchmark>_<mode>_compute_log
        logs/<config>/<benchmark>_<mode>_data_movement_log
    """
    pattern = "**/*" + _LOG_SUFFIX if recursive else "*" + _LOG_SUFFIX
    for compute_path in sorted(log_dir.glob(pattern)):
        stem = compute_path.name[: -len(_LOG_SUFFIX)]

        mode = "unknown"
        benchmark = stem
        for candidate in ("unfused", "fused"):
            suffix = "_" + candidate
            if stem.endswith(suffix):
                mode = candidate
                benchmark = stem[: -len(suffix)]
                break

        datamov_path = compute_path.with_name(stem + "_data_movement_log")
        config = compute_path.parent.name
        yield benchmark, config, mode, compute_path, datamov_path


# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log_dir", type=Path,
                    help="directory of logs (e.g. logs/PIMeval_Bank_LPDDR)")
    ap.add_argument("--csv", type=Path, help="write summary CSV here")
    ap.add_argument("--commands-csv", type=Path,
                    help="write long-form per-command breakdown here")
    ap.add_argument("--recursive", action="store_true",
                    help="descend into per-config subdirectories")
    args = ap.parse_args(argv)

    if not args.log_dir.is_dir():
        print(f"error: not a directory: {args.log_dir}", file=sys.stderr)
        return 2

    records: list[dict] = []
    cmd_records: list[dict] = []
    skipped = 0

    for benchmark, config, mode, compute_path, datamov_path in discover(
            args.log_dir, args.recursive):

        # Deliberately not wrapped in a blanket try/except: a missing or
        # malformed log is reported, not silently dropped into a short CSV.
        if not datamov_path.exists():
            print(f"warn: no data-movement log for {benchmark} "
                  f"[{config}/{mode}]: expected {datamov_path.name}",
                  file=sys.stderr)
            skipped += 1
            continue

        compute = parse_log(compute_path)
        datamov = parse_log(datamov_path)

        if not compute.complete:
            print(f"warn: {compute_path.name} has no PIM Command Stats block "
                  f"(run likely failed)", file=sys.stderr)
            skipped += 1
            continue

        records.append(combine(compute, datamov, benchmark, config, mode))
        cmd_records.extend(command_rows(compute, benchmark, config, mode))

    if not records:
        print("error: no complete log pairs found", file=sys.stderr)
        return 1

    # Union of keys, with the identifying columns pinned to the front.
    lead = ["benchmark", "config", "mode",
            "total_runtime_ms", "total_energy_mj",
            "compute_runtime_ms", "compute_energy_mj",
            "data_movement_runtime_ms", "data_movement_energy_mj",
            "cmd_count", "fused_groups"]
    rest = sorted({k for r in records for k in r} - set(lead))
    columns = lead + rest

    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="") as fh:
            writer = csv.DictWriter(fh, fieldnames=columns, restval="")
            writer.writeheader()
            writer.writerows(records)
        print(f"wrote {len(records)} rows -> {args.csv}")
    else:
        writer = csv.DictWriter(sys.stdout, fieldnames=columns, restval="")
        writer.writeheader()
        writer.writerows(records)

    if args.commands_csv and cmd_records:
        args.commands_csv.parent.mkdir(parents=True, exist_ok=True)
        with args.commands_csv.open("w", newline="") as fh:
            writer = csv.DictWriter(
                fh, fieldnames=["benchmark", "config", "mode", "cmd_name",
                                "cnt", "runtime_ms", "energy_mj", "gops_per_w"])
            writer.writeheader()
            writer.writerows(cmd_records)
        print(f"wrote {len(cmd_records)} command rows -> {args.commands_csv}")

    if skipped:
        print(f"note: skipped {skipped} incomplete run(s)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
