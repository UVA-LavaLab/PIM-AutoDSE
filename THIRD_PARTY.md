# Third-party components

PIM-AutoDSE is MIT licensed (see [`LICENSE`](LICENSE)). It incorporates and
depends on the components below.

## Submodules

| Component | Upstream | License | Pinned to |
|---|---|---|---|
| **MISAAL** | https://github.com/RafaeNoor/MISAAL | Apache-2.0 | branch `pim-fused` |
| **Hydride** | https://github.com/akothen/Hydride | see upstream | branch `bitserial` |

MISAAL in turn vendors **Halide** (MIT, © 2012–2020 MIT CSAIL, Google,
Facebook, Adobe, NVIDIA and other contributors) under
`frontends/halide/`, with local modifications to `src/CodeGen_C.cpp`,
`src/Rosette.cpp`, and an added `src/misaal.cpp`.

## Vendored code

### bitsimd ISA enumerator — `isa/`

Copied from `codegen-generator/targets/DRAM_BitSIMD` of
PiMCOM (`github.com/RafaeNoor/PiMCOM` - **not currently public**), which is authoritative for
bitsimd. PiMCOM is itself derived from Hydride's `codegen-generator`.

> **Action required before release:** PiMCOM carries no LICENSE file and no
> per-file headers, so its code is by default all-rights-reserved. A license
> must be added upstream (or the vendored files given explicit headers) before
> this directory can ship under MIT.

### hannk — `benchmarks/hannk/`

Halide's `apps/hannk` TFLite interpreter, MIT licensed, inherited from Halide.
These files carry no LICENSE of their own upstream; attribution is recorded
here.

- Upstream: https://github.com/halide/Halide/tree/main/apps/hannk
- License: MIT, © 2012–2020 MIT CSAIL, Google, Facebook, Adobe, NVIDIA
  CORPORATION, and other contributors

### PIMeval / PIMbench — `libpimeval/`, `configs/`

- Upstream: https://github.com/UVA-LavaLab/PIMeval-PIMbench
- License: MIT, © 2024 University of Virginia

`configs/asplos/{DDR4,GDDR5,HBM2}*.ini` and the benchmark datasets under
`PIMbench/` derive from that project.

> **Note:** upstream `libpimeval.h` states "See the LICENSE file in the root of
> this repository". PIM-AutoDSE's own `LICENSE` is MIT with the same UVA
> copyright, which satisfies this. Prebuilt `libpimeval.a` archives should
> **not** be redistributed without the accompanying license text — build from
> source instead.

## External dependencies (not redistributed)

| Component | Purpose | License |
|---|---|---|
| **LLVM** 12–15 | Required by Halide | Apache-2.0 with LLVM exceptions |
| **Racket / Rosette** | ISA regeneration only | MIT / LGPL |
| **egglog** | Equality saturation | MIT |
| **OpenTuner** | DSE search | MIT |
| **DRAMsim3** | DRAM timing model, via PIMeval | MIT |

## Datasets

Benchmark inputs under `PIMbench/*/Dataset/` and
`misc-bench/*/dataset*/` originate with PIMeval-PIMbench. Individual datasets
may carry their own terms; consult upstream before redistribution.
