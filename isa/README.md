# PIM CISC ISA Enumerator

Utilities for enumerating a CISC ISA for PIM with formal semantics, built out
of RISC-like ISA building blocks. "Fusion" here means constructing a CISC ISA
from simpler primitives — not loop fusion.

> **Provenance, not a build step.** The ISA in this release is already
> enumerated and the rewrite rules are already synthesized. Nothing under
> `isa/` needs to run to build or execute the benchmarks; it is here so the
> generated artifacts can be audited and regenerated. Regenerating requires
> Racket/Rosette and the Hydride submodule — see [Regenerating](#regenerating).

## Layout

| Path | Contents |
|---|---|
| `spec/` | ISA descriptions as TOML: `auto.toml` (every enumerated fused instruction, ~28 MB), and the hand-written base ISA files |
| `gen/` | The enumerator: `Ops.py`, `Enumerator.py`, `TOMLEmitter.py`, `PIM_API_UTILS.py`, `PIM_TUNER_UTILS.py`, `GenRewriteRules.py` |
| `rewrite_rules/` | Pre-synthesized rewrite rules (`rewrite_rules.txt`, `manual_rewrite_rules.txt`, `copy_rewrite_rules.txt`) |
| `lowering/` | Generated `fused_lower.h` / `unfused_lower.h` — see `lowering/README.md` |
| `perf_cost_model/` | `GenPimFusedCost.py`, `AnalyzePerf.py` — builds the per-target cost model |
| `parser/` | Pseudocode → AST → Rosette (`DRAM_PseudoCode_Parser.py`, `DRAM_AST.py`, `DRAM_Rosette_Emitter.py`) |
| `microcode/` | Rosette microcode models (`interpreter.rkt`, `cost.rkt`) |
| `libpimsim/` | Per-operation TOML descriptions (`pimAdd.toml`, `pimMul.toml`, …) |

## Pipeline

```
Ops.py            primitive building blocks (add, truncate, cond-select, broadcast, …)
    │
    ▼  Enumerator.py        stitch primitives into CISC candidates
    │
    ▼  TOMLEmitter.py       emit pseudocode + semantics per instruction
spec/*.toml
    │
    ├──▼  fused_lower.h     interface telling the simulator to model the op WITH fusion
    └──▼  unfused_lower.h   same op WITHOUT fusion, for the baseline
                │
                ▼  GenPimFusedCost.py
            per-target cost model CSV  →  consumed by the MISAAL compiler
```

### 1. Enumeration

[`gen/Ops.py`](gen/Ops.py) describes the primitive building blocks to combine
when constructing more complex CISC ISAs — element-wise addition, truncate,
conditional select, broadcasts, and so on.

[`gen/Enumerator.py`](gen/Enumerator.py) stitches those primitives together.
You supply the element bitwidths and vector-size combinations to generate
from. The methodology uses the formal-methods machinery from MISAAL to
abstract properties of these instructions into a parameterized,
target-agnostic IR — so the generated compiler is **vector-size agnostic**,
and the full range of vector sizes does not have to be enumerated explicitly.

### 2. TOML emission

[`gen/TOMLEmitter.py`](gen/TOMLEmitter.py) emits one entry per generated
instruction, carrying its pseudocode semantics:

```toml
[test_enum_2.comb_16_fused_pim_op_70]
name = "comb_16_fused_pim_op_70"
operand_sizes = [512, 8, 512, 512]
operand_layouts = ['DRAM_VERT', 'DRAM_VERT', 'DRAM_VERT', 'DRAM_VERT']
operand_elem_bw = [8, 8, 8, 8]
result_layout = "DRAM_VERT"
result_size = 64
result_elem_bw = 1
signedness = 1
semantics = """\
DEFINE comb_16_fused_pim_op_70(a, b, c, d):
FOR idx IN RANGE(0, 64, 1):
var_0_low = idx * 8
var_1_low = 0 * 8
var_2 = a[var_0_low+7:var_0_low] >> b[var_1_low+7:var_1_low]
var_3_low = idx * 8
var_4_low = idx * 8
var_5 = c[var_3_low+7:var_3_low] - d[var_4_low+7:var_4_low]
var_6_low = idx * 1
dst[var_6_low+0:var_6_low] = (var_2 < var_5) ? 1 : 0
ENDFOR
"""
```

These files contain thousands of instructions with *similar* but not
**equivalent** semantics. PIM-AutoDSE later folds the similar ones into an
abstracted AutoLLVM IR representation.

### 3. Lowering interfaces

Alongside the pseudocode, two C++ interfaces to the simulator are emitted.

`fused_lower.h` instructs the simulator to model the operation **with** the
optimizations fusion affords — the whole expression is submitted as one
`PimFusionBlock` and closed with `pimFuse`, so intermediates stay inside the PIM unit:

```c++
void comb_16_fused_pim_op_70(void* reg_0, int64_t reg_0_num_elems, ...) {
    PimFusionBlock prog;
    PimObjId fuse_root   = pimAlloc(PIM_ALLOC_AUTO, reg_0_num_elems, PIM_INT8);
    PimObjId fuse_expr_0 = pimAllocAssociated(fuse_root, PIM_BOOL);
    ...
    prog.add(pimCopyHostToDevice, (void*)reg_0, fuse_expr_2, 0UL, 0UL);
    // Scalar operand reg_1 needs no copy to PIM memory
    prog.add(pimSub,            fuse_expr_5, fuse_expr_6, fuse_expr_4);
    prog.add(pimShiftBitsRight, fuse_expr_2, fuse_root,   (unsigned) fuse_expr_3);
    prog.add(pimLT,             fuse_root,   fuse_expr_4, fuse_expr_0);
    prog.add(pimCopyDeviceToHost, fuse_expr_0, (void*)ret_vec, 0UL, 0UL);
    pimFuse(prog);
    ...
}
```

`unfused_lower.h` emits the same operation **without** those optimizations, so
the runtime and energy difference attributable to fusion can be measured
directly. The benchmark build selects between them with `FUSED=1` / `FUSED=0`;
each unfused intermediate spills back to the DRAM array, costing an
ACTIVATE/PRECHARGE pair.

### 4. Cost model

[`perf_cost_model/GenPimFusedCost.py`](perf_cost_model/GenPimFusedCost.py)
builds the harness that configures the PIM simulator, invokes each CISC
instruction with fusion enabled and disabled, and records the metrics into a
per-target `.csv`. The MISAAL compiler reads that CSV
(`COST_FILE_CSV_NAME`) to decide which instruction variants to select.

The benchmark build generates a missing CSV on demand. To generate every CSV
the benchmarks need up front (each configuration in `benchmarks/configs/` at
each benchmark VF from `benchmarks/Makefile`):

```sh
python3 perf_cost_model/generate_config_files_pim_configs.py --dry-run   # list them
python3 perf_cost_model/generate_config_files_pim_configs.py --jobs 4
```

## Environment

Everything here resolves paths from the environment rather than a fixed
checkout. Source `env.sh` at the repository root first:

```sh
source env.sh
```

Relevant variables: `HALIDE_DISTRIB`, `PIM_ISA_ROOT`, `PIM_EVAL_ROOT`,
`PIM_PERF_RESULTS_DIR`.

## Regenerating

Regeneration is **not** required to use this repository. It needs Racket and
Rosette plus the Hydride submodule, which supplies the Rose IR
(`RoseAbstractions`, `RoseContext`, `RoseTypes`, …) that `parser/` imports:

```sh
git submodule update --init Hydride
source env.sh
cd gen && python3 Enumerator.py  # writes auto.toml, fused_lower.cpp, unfused_lower.cpp,
                                 # get_perf_stats.cpp and rewrite_rules.txt into the cwd
cd ..
python3 gen/GenRewriteRules.py # synthesize -> rewrite_rules/
```

## Provenance

This code is vendored from the `codegen-generator/targets/DRAM_BitSIMD` tree of
PiMCOM (`github.com/RafaeNoor/PiMCOM` - **not currently public**), which is authoritative for
bitsimd. A parallel copy exists in the MISAAL submodule at
`targets/pim_fused/codegen-generator/`; where the two differ, **this copy
wins** — it carries the DRAM-command event counters (`evt_act`, `evt_pre`,
`evt_cas`, `evt_compute`) that the fusion analysis depends on.

Generated artifacts excluded from version control here, because they are large
and reproducible from the above: `fused_lower.cpp` (105 MB),
`unfused_lower.cpp` (100 MB), `fused_ops.pickle` (13 MB), `semantics.py` (42 MB),
and prebuilt `libpimeval.a` archives. `auto.toml` *is* included, in `spec/`, as
the description of every fused instruction the lowering headers implement.
