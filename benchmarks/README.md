# Benchmarks

One benchmark tree, retargeted by config file. Previously this was five
near-duplicate directories (`Aquabolt`, `PIMeval_Bank_LPDDR`,
`PIMeval_Bank_Rank1_GDDR`, `PIMeval_Bank_Rank1_HBM`, `PIMeval_Bank_Rank20`)
that differed only in which config they pointed at.

## Running

```sh
source ../env.sh

python3 run_benchmarks.py --list
python3 run_benchmarks.py --benchmark axpy
python3 run_benchmarks.py --benchmark axpy --config PIMeval_Bank_Rank20 --mode fused
python3 run_benchmarks.py --all --csv ../results/sweep.csv
```

A full sweep is 18 benchmarks × 6 configs × 2 modes = 216 runs and takes
**hours**. Start with `--dry-run`.

Useful flags: `--keep-going` (don't stop at the first failure), `--timeout SEC`,
`--clean` (rebuild from scratch each run), `--jobs N`.

Or drive `make` directly:

```sh
make axpy CONFIG=PIMeval_Bank_LPDDR FUSED=1
make list
make clean
```

## The two axes

**Hardware target** — `CONFIG=<name>`, a basename in `configs/`. The target is
read at run time from `$PIM_CONFIG`, so one build serves every target.

| Config | Target |
|---|---|
| `PIMeval_Bank_LPDDR` | Bank-level, 10 ranks, LPDDR4 |
| `PIMeval_Bank_Rank20` | Bank-level, 20 ranks, DDR4 |
| `PIMeval_Bank_Rank1_GDDR` | Bank-level, 1 rank, GDDR5 |
| `PIMeval_Bank_Rank1_HBM` | Bank-level, 1 rank, HBM2 |
| `PIMeval_AiM_Rank8` | AiM, 8 ranks, GDDR5 |
| `Aquabolt` | Aquabolt, 8 ranks, HBM2 — evaluated on axpy, convolution, gemm, gemv and relu only |

**Fusion mode** — `FUSED=1` compiles against `isa/lowering/fused_lower.h`,
`FUSED=0` against `unfused_lower.h`. Both link the same freshly built
`libpimeval.a`, so the delta is attributable to fusion alone. Internally this
drives `MISAAL_NO_FUSION`.

## Cost model

MISAAL chooses which fused ISA variants to emit using a cost model measured for
a specific hardware config and vectorization factor (VF):

```
isa/perf_cost_model/perf_logs/pim_perf_results_config_<CONFIG>_vf<VF>.csv
```

Before MISAAL codegen, `common/ensure_cost_model.py` reuses that CSV if it
exists and otherwise generates it with `isa/perf_cost_model/GenPimFusedCost.py`,
which runs every fused and unfused ISA operation on the simulator. Generation
is slow the first time for each (config, VF) and cached afterwards; the CSVs
are not committed. A lock file keeps parallel builds from generating the same
CSV twice. The generator must use this repository's `libpimeval` build (staged
in `libpimsim/` by `setup.sh`) and refuses to run against any other copy.

Each benchmark's VF comes from the `vectorize()` factor in its generator and is
listed by `make list`; override it with `VF=<n>`. The step is skipped when
`ENABLE_HYDRIDE=0`, since that path does not use MISAAL.

## Build pipeline

```
<bench>/src/<bench>_generator.cpp
    │  1. compile the Halide generator
    ▼
<bench>/bin/<bench>_generator
    │  2. run it -> MISAAL/PIM lowering
    ▼
<bench>/bin/<bench>.halide_generated.cpp
    │  3. inline.py        splice in the PIM header
    │  4. fix_type_decl.py  repair generated signatures
    ▼
    │  5. compile twice: compute-profiling and data-movement-profiling
    ▼
<bench>/bin/<bench>_{compute,data_movement}_run.out
    │  6. run both under $PIM_CONFIG
    ▼
logs/<config>/<bench>_<mode>_{compute_log,data_movement_log,pim_header.h}
```

## Output

Each `(benchmark, config, mode)` produces two logs:

- `*_compute_log` — full run; `PIM Command Stats` carries the per-command cost
- `*_data_movement_log` — compute suppressed; `Data Copy Stats` isolate the
  host↔device transfer cost

End-to-end cost is the sum. Both logs contain a copy block, so the parser takes
it only from the data-movement log to avoid double counting.

```sh
python3 stats/parse_logs.py logs --recursive --csv ../results/sweep.csv \
                                 --commands-csv ../results/commands.csv
```

The summary CSV carries device parameters, transfer bytes and cost, per-command
totals, fusion group count, and combined runtime/energy. The commands CSV is
the long-form per-instruction breakdown.

A run that exits cleanly but emits no `PIM Command Stats` block is reported,
not silently dropped.

## Benchmarks

`tensor_add`, `axpy`, `relu`, `gemv_v1`, `gemv_v2`, `gemv_v3`, `gemm_small`,
`gemm_medium`, `gemm_large`, `batched_gemm_v1`, `bitsimd_gemv`, `histogram`,
`filter_by_key`, `convolution`, `max_pool`, `radix_sort`, `radix_sort_i16`,
`softmax`.

`run_benchmarks.py --list` reads this list from the Makefile, so the two cannot
drift apart.
