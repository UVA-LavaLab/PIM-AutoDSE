# PIM-AutoDSE

![Contribution](https://img.shields.io/badge/Contribution-Welcome-blue)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A retargetable framework for **automated ISA and design space exploration for
processing-in-memory (PIM)**.

Processing-in-memory offers a promising answer to the memory wall, but its
Instruction Set Architecture has never been systematically evaluated. ISA
choices — instruction fusion in particular — directly affect DRAM read/write
behaviour: an *unfused* instruction spills its intermediate result back to the
DRAM array, and every spill costs an ACTIVATE/PRECHARGE pair that inflates both
runtime and energy.

PIM-AutoDSE extends [PIMeval-PIMbench](https://github.com/UVA-LavaLab/PIMeval-PIMbench)
with cycle-accurate performance and energy analysis covering instruction
fusion, register contention, and prefetching; extends the synthesis-based
compiler [MISAAL](https://github.com/RafaeNoor/MISAAL) to generate code from
high-level languages to diverse PIM architectures; and adds a methodology for
synthesizing PIM ISAs from pseudocode to produce RISC- and CISC-like ISAs
automatically.

Using it, we present the first systematic study of instruction fusion for PIM,
showing up to **2.7× improvement in runtime and energy** over non-fused
baselines.

## Architecture

Three repositories cooperate; two arrive as submodules.

```
PIM-AutoDSE/                  simulator, benchmarks, ISA, DSE
├── libpimeval/               the PIM simulator
├── isa/                      bitsimd ISA enumeration -> lowering interfaces
├── benchmarks/               one tree, retargeted by config file
├── dse/                      design space exploration sweeps
├── MISAAL/        submodule  Halide-based compiler targeting the simulator
├── Hydride/       submodule  Rose / Rosette IR framework
└── egglog/        submodule  equality saturation engine (pinned to 14542d7)
```

```
ISA enumeration (TOML)  ──>  fused_lower.h / unfused_lower.h
                                      │  (shipped; pick one per build)
MISAAL Halide generator               │
        │                             │
        ▼                             │
<bench>.halide_generated.cpp          │
        │                             │
        ▼  fix_type_decl.py           │
   repaired misaal_* signatures       │
        │                             │
        └──────────► compile + link ◄─┘
                          │   + libpimeval.a
                          ▼
                 run under <config>.cfg
                          │
                          ▼
        *_compute_log + *_data_movement_log  ──>  results CSV
```

## Quick start

```sh
git clone --recursive https://github.com/UVA-LavaLab/PIM-AutoDSE.git
cd PIM-AutoDSE

# Builds Halide and libpimeval, writes env.sh
LLVM_CONFIG=/path/to/llvm-12/bin/llvm-config ./setup.sh
source env.sh

cd benchmarks
python3 run_benchmarks.py --list
python3 run_benchmarks.py --benchmark axpy --config PIMeval_Bank_LPDDR
python3 run_benchmarks.py --all --csv ../results/sweep.csv
```

A full sweep compiles and simulates every benchmark × config × mode and can
take **hours**. Use `--dry-run` first to see the plan.

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| LLVM | 12–15 | Reference build used **LLVM 12**. Point `LLVM_CONFIG` at it; `setup.sh` will not build LLVM for you. |
| GNU Make | any | Halide is built with `make distrib`, **not** CMake — the MISAAL fork's PIM codegen path is only wired into the Makefile |
| GCC / G++ | C++17 | AVX-512 used by the benchmark harness |
| Python | ≥ 3.8 | `pip install -r requirements.txt` |
| Git LFS | any | Required for the ISA lowering headers (~190 MB) |
| Rust / cargo | any | Builds egglog, the equality-saturation engine |
| Racket + Rosette | optional | Only to *regenerate* the ISA; the release ships pre-generated artifacts |

## Selecting a hardware target

One benchmark tree serves every target; the config file is the only knob.
The target is read at run time from `$PIM_CONFIG`, so a build is not tied to
a target.

```sh
make axpy CONFIG=PIMeval_Bank_Rank20 FUSED=1
```

Available configs live in `benchmarks/configs/`:
`PIMeval_Bank_LPDDR`, `PIMeval_Bank_Rank20`, `PIMeval_Bank_Rank1_GDDR`,
`PIMeval_Bank_Rank1_HBM`, `PIMeval_AiM_Rank8`, `Aquabolt`.

`FUSED=1` compiles against the fused ISA lowering, `FUSED=0` against the
unfused baseline. Both link the same freshly built `libpimeval.a`, so the
difference between them is attributable to fusion alone.

## Results

Each run produces a compute log and a data-movement log. Parse them with:

```sh
python3 benchmarks/stats/parse_logs.py benchmarks/logs --recursive --csv out.csv
```

The CSV carries device parameters, host↔device transfer bytes and cost,
per-command runtime/energy/GOPS-per-watt, the fusion group count, and combined
totals.

## Documentation

- [`isa/README.md`](isa/README.md) — how the ISA is enumerated and lowered
- [`benchmarks/README.md`](benchmarks/README.md) — building and running benchmarks
- [`dse/README.md`](dse/README.md) — design space exploration sweeps
- [`RESTRUCTURE.md`](RESTRUCTURE.md) — release engineering notes and open items

## Citation

See [`CITATION.cff`](CITATION.cff).

## Contact

* Abdul Rafae Noor — arnoor2 AT illinois DOT edu
* Farzana Ahmed Siddique — farzana AT virginia DOT edu
* Kevin Skadron — skadron AT virginia DOT edu
* Vikram Adve — vadve AT illinois DOT edu

## License

MIT — see [`LICENSE`](LICENSE). Third-party components and their licenses are
listed in [`THIRD_PARTY.md`](THIRD_PARTY.md).
