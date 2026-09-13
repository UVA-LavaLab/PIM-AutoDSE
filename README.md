# PIM-AutoDSE

![Contribution](https://img.shields.io/badge/Contribution-Welcome-blue)
![License](https://img.shields.io/badge/license-MIT-green.svg)

PIM-AutoDSE is a framework for automated instruction set architecture (ISA)
and design space exploration for processing-in-memory (PIM).

It combines:

- **A PIM simulator** based on [PIMeval-PIMbench](https://github.com/UVA-LavaLab/PIMeval-PIMbench),
  extended with performance and energy modeling for instruction fusion,
  register contention, and prefetching.
- **A compiler flow** built on [MISAAL](https://github.com/RafaeNoor/MISAAL),
  which lowers Halide programs to fused or unfused PIM instructions.
- **An ISA synthesis methodology** that generates RISC- and CISC-style PIM
  instructions from pseudocode descriptions.

## Requirements

- Linux (x86-64 with AVX-512)
- GCC with C++17 support and GNU Make
- LLVM 12–15 (tested with LLVM 12)
- Python 3.8+
- Rust and Cargo (to build egglog)
- [Git LFS](https://git-lfs.com/)

## Installation

Install Git LFS before cloning, since the ISA lowering headers are stored with it:

```sh
git lfs install
git clone --recursive https://github.com/UVA-LavaLab/PIM-AutoDSE.git
cd PIM-AutoDSE
pip install -r requirements.txt
```

Build the toolchain and generate the environment script, pointing
`LLVM_CONFIG` at your LLVM installation:

```sh
LLVM_CONFIG=/path/to/llvm/bin/llvm-config ./setup.sh
source env.sh
```

`setup.sh` builds Halide (from the MISAAL submodule), egglog, the simulator
library, and the ISA lowering libraries, then writes `env.sh`. The first run
takes a while. Run `./setup.sh --help` for options such as `--skip-halide`.

Source `env.sh` in every new shell before building or running benchmarks.

## Usage

Run a benchmark on a PIM configuration:

```sh
cd benchmarks
python3 run_benchmarks.py --list
python3 run_benchmarks.py --benchmark relu --config PIMeval_Bank_LPDDR
```

Collect the results into a CSV:

```sh
python3 stats/parse_logs.py logs --recursive --csv ../results/results.csv
```

See [`benchmarks/README.md`](benchmarks/README.md) for configurations, fusion
modes, cost models, and output formats.

## Repository layout

| Path | Contents |
|---|---|
| `libpimeval/` | PIM simulator library |
| `benchmarks/` | Halide benchmarks, hardware configurations, and scripts to run them and parse results |
| `isa/` | ISA specification, enumeration and code generation, and the fused/unfused lowering headers |
| `dse/` | Design space exploration scripts |
| `MISAAL/` | Compiler (submodule) |
| `Hydride/` | Rosette IR framework used for ISA synthesis (submodule) |
| `egglog/` | Equality saturation engine used by MISAAL (submodule) |
| `PIMbench/`, `misc-bench/` | Benchmarks from PIMeval-PIMbench |

Further documentation:

- [`benchmarks/README.md`](benchmarks/README.md) — running benchmarks
- [`isa/README.md`](isa/README.md) — ISA enumeration and lowering
- [`dse/README.md`](dse/README.md) — design space exploration

## Citation

If you use PIM-AutoDSE in your research, please cite
*PIM-AutoDSE: Automated ISA and Design Space Exploration for PIM*.
Citation metadata is in [`CITATION.cff`](CITATION.cff).

## Contact

- Abdul Rafae Noor — arnoor2 AT illinois DOT edu
- Farzana Ahmed Siddique — farzana AT virginia DOT edu
- Kevin Skadron — skadron AT virginia DOT edu
- Vikram Adve — vadve AT illinois DOT edu

## License

PIM-AutoDSE is released under the MIT License. See [`LICENSE`](LICENSE), and
[`THIRD_PARTY.md`](THIRD_PARTY.md) for third-party components.
