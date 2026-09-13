# Design space exploration

Utilities for sweeping PIM hardware configurations and tabulating the results.
These drive the design-space exploration behind the paper's ISA analysis.

Run them from this directory (`cd dse`), with the environment sourced:

```sh
source ../env.sh
```

## Scripts

| Script | Role |
|---|---|
| `sweep.py` | The main sweep driver (was `quick.py`) |
| `dse_compile.py` | Compiles one configuration's benchmark binaries |
| `process_dse.py` | Log directory → CSV, one row per configuration |
| `process_dse_insts.py` | Instruction-mix histogram across a sweep's generated headers |
| `generate_cost.py` | Builds the per-configuration cost model |
| `generate_regsweep_cfg_files.py` | Generates register-sweep `.cfg` variants |
| `cost_dict.py` | Cost lookup tables |
| `get_eq_class_for.py` | Equivalence-class lookup for a given instruction |

## Evaluating the eval configurations

`dse_compile.py` runs the DSE experiment: every benchmark it lists on every
configuration in `cfgs/autodse_eval_cfgs/`. For each (benchmark, config) pair it

1. reuses or generates the cost model in `../isa/perf_cost_model/perf_logs/`
   (the same CSVs the benchmark flow uses),
2. compiles the benchmark from `../benchmarks/` with MISAAL against that cost
   model, using the lowering objects built by `setup.sh`,
3. runs the compute and data-movement binaries with `PIM_CONFIG` set to the
   configuration, writing `<config>_<bench>_<VF>_compute_log`, `..._data_log`
   and `..._header.h` to `autodse_main_out/`.

```sh
cd dse
python3 dse_compile.py
python3 process_dse.py --log-dir autodse_main_out --benchmark softmax --data-suffix _data_log
```

## Configurations

Only the five evaluation configs are tracked, in
`cfgs/autodse_eval_cfgs/` (used by `dse_compile.py`).

The full sweep configs (~1,100 generated `.cfg` files: `cfg_files/`,
`cfg_files_v2/`, `cfg_files_scalar_sweep/`, `cfg_reg_sweep_files/`,
`aqbolt_cfg_files/`) are not in the repository. `generate_regsweep_cfg_files.py`
derives the register-sweep variants from `cfg_files/`.

Each references its DRAM timing `.ini` as
`../../../benchmarks/configs/<name>.ini`. libpimeval tries that path relative
to the working directory first and then relative to the `.cfg` file's own
directory, so it resolves from anywhere. The scripts locate `cfgs/` relative
to themselves; they still write their logs and CSVs to the working directory.

## Tabulating results

```sh
python3 process_dse.py --log-dir dse_logs_perf_opt --benchmark histogram
```

Pairs each `*_compute_log` with its `*_data_movement_log`, extracts the PIM
stats, and writes one row per configuration.

Older log directories used `_data_log` instead; pass
`--data-suffix _data_log` for those. Use `--strict` to fail the run if any
configuration cannot be parsed.

> For the benchmark flow (as opposed to a DSE sweep), prefer
> [`../benchmarks/stats/parse_logs.py`](../benchmarks/stats/parse_logs.py),
> which parses the same logs without needing the tuner dependencies and emits
> a richer schema.

## Results

`../results/` is a local output directory for sweep and benchmark CSVs. It is
gitignored: write results there, but they are not committed.

## A note on provenance

These were recovered from a 5.9 GB working directory that mixed source with
run artifacts — roughly 200 `.egg` solver dumps, ~90 MB simulator logs per run,
55 MB compiled binaries, and per-run work directories. Only the scripts and the evaluation
configurations are kept here; `.gitignore` at the repository
root excludes the artifact patterns so they do not creep back in.
