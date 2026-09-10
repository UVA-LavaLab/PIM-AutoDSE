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
| `Aquabolt` | Aquabolt, 8 ranks, HBM2 |

**Fusion mode** — `FUSED=1` compiles against `isa/lowering/fused_lower.h`,
`FUSED=0` against `unfused_lower.h`. Both link the same freshly built
`libpimeval.a`, so the delta is attributable to fusion alone. Internally this
drives `MISAAL_NO_FUSION`.

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
    │  4. fix_type_decl.py REQUIRED - see below
    ▼
    │  5. compile twice: compute-profiling and data-movement-profiling
    ▼
<bench>/bin/<bench>_{compute,data_movement}_run.out
    │  6. run both under $PIM_CONFIG
    ▼
logs/<config>/<bench>_<mode>_{compute_log,data_movement_log,pim_header.h}
```

### Why `fix_type_decl.py` is not optional

Halide emits the `misaal_*` intrinsic **definitions** with incorrect parameter
types. `common/fix_type_decl.py` recovers the correct types from the forward
declarations — first pass builds a map from the `;` declarations, second pass
rewrites the `{` definitions to match. Without it the generated C++ does not
compile against the lowering header. It is a hard dependency of the compile
step in the Makefile, not a wrapper you can skip.

It rewrites the `.cpp` in place, which is safe because `make clean` removes
`<bench>/bin/` and the file is regenerated on every build.

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

## Threading and `test/stubs.cpp`

libpimeval runs its simulation on a thread pool sized to the host by default
(`Number of Threads = 32` on a 32-core machine). **Multi-threaded is the
default and is what you want** - it is verified correct at 1, 8 and 32
threads, all producing identical results.

To pin the thread count (for determinism or to limit load):

```sh
# in the .cfg
max_num_threads = 1
# or in the environment
PIMEVAL_MAX_NUM_THREADS=1 make axpy
```

`test/stubs.cpp` is **not linked**, deliberately. It no-ops the whole pthread
API to force single-threaded execution, but:

1. It does not stub `pthread_create`, so libpimeval still starts its thread
   pool while every mutex, condvar and TLS call is a no-op - a data race.
2. Its `pthread_once` returns without running the initialiser. libstdc++
   initialises the locale lazily through `pthread_once`, and a definition in
   the executable interposes on the one libstdc++ itself calls. The locale
   globals are therefore never constructed, and the first `std::ifstream` -
   reached from `pimCreateDeviceFromConfig` via `pimUtils::readFileContent` -
   segfaults inside `std::ctype<char>::ctype`.

Point 2 explains a long-standing puzzle: switching libpimeval's output from
`std::cout` to `printf` appeared to fix crashes. `printf` never touches
`std::locale`, so it removed those crash sites - but it treated the symptom.
Any remaining stream (the config-file `ifstream`) still crashed. Verified
directly: with the broken `pthread_once`, a `printf`-only program exits 0
while `std::ifstream` and `std::cout` both segfault.

Use `max_num_threads` for single-threaded runs instead of the stubs.

## Benchmarks

`tensor_add`, `axpy`, `relu`, `gemv_v1`, `gemv_v2`, `gemv_v3`, `gemm_small`,
`gemm_medium`, `gemm_large`, `batched_gemm_v1`, `bitsimd_gemv`, `histogram`,
`filter_by_key`, `convolution`, `max_pool`, `radix_sort`, `radix_sort_i16`,
`softmax`.

`run_benchmarks.py --list` reads this list from the Makefile, so the two cannot
drift apart.
