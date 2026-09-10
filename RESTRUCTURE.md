# PIM-AutoDSE — Open-Source Restructuring Plan

Working document for the public release of PIM-AutoDSE, the artifact for
*"PIM-AutoDSE: Automated ISA and Design Space Exploration for PIM"*
(Noor, Siddique, Kothari, Guo, Skadron, Adve — UIUC / UVA).

Branch: `opensource-prep` (off `main`).

---

## 1. Architecture

Three repositories, one checkout:

| Component | Source | Mechanism |
|---|---|---|
| Hardware simulator (`libpimeval`) | PIM-AutoDSE | in-repo |
| bitsimd ISA codegen + lowering | PiMCOM `codegen-generator/targets/DRAM_BitSIMD` | **vendored** (copy-in) |
| Rose / Rosette IR framework | `AkashIwnK/Hydride` @ `bitserial` | submodule |
| Halide compiler + frontend | `RafaeNoor/MISAAL` @ `pim-fused` | submodule |

PiMCOM is *not* a submodule — its bitsimd tree is copied in once and PiMCOM
stops being a dependency. PiMCOM's copy is authoritative for bitsimd (it is
ahead of MISAAL's `targets/pim_fused/codegen-generator/` on all five diverged
files, including the `evt_act` / `evt_pre` / `evt_cas` / `evt_compute`
DRAM-command counters the paper's fusion analysis rests on).

### End-to-end flow

```
ISA enumeration (TOML)  ──[codegen-generator]──>  fused_lower.h / unfused_lower.h
                                                         │  (shipped, not built)
MISAAL Halide generator                                  │
        │                                                │
        ▼                                                │
<bench>/bin/<bench>.halide_generated.cpp                 │
        │                                                │
        ▼  fix_type_decl.py   (REQUIRED)                 │
   repaired misaal_* signatures                          │
        │                                                │
        └───────────────► compile + link ◄───────────────┘
                                │        + libpimeval.a  (built from source)
                                ▼
                     run under <config>.cfg
                                │
                                ▼
        <bench>_<mode>_compute_log + <bench>_<mode>_data_movement_log
                                │
                                ▼  stats parser
                             results CSV
```

`fix_type_decl.py` is a hard dependency of the compile step, not an optional
wrapper: Halide emits the `misaal_*` intrinsic *definitions* with wrong
parameter types, and the script recovers the correct types from the forward
declarations. Without it the generated C++ does not compile.

---

## 2. Target layout

```
PIM-AutoDSE/
├── README.md                    # what it is, paper ref, quickstart
├── LICENSE                      # MIT (existing)
├── CITATION.cff
├── setup.sh                     # submodules → Hydride → Halide → libpimeval
├── requirements.txt             # ply, toml, opentuner, pandas, numpy
├── .gitmodules .gitattributes .gitignore
│
├── libpimeval/                  # simulator — existing, untouched
├── configs/                     # hardware target configs — existing
│
├── isa/                         # ← vendored from PiMCOM DRAM_BitSIMD
│   ├── README.md                #   ported from MISAAL's copy (7.3 KB)
│   ├── spec/                    #   ISA enumeration TOMLs
│   ├── parser/                  #   DRAM_AST, PseudoCode_Parser,
│   │                            #   Rosette_Emitter, TOML_Utils
│   ├── gen/                     #   FusedOpGen: Enumerator, Ops, Canonical,
│   │                            #   TOMLEmitter, PIM_API_UTILS, GenRewriteRules
│   └── lowering/                #   ← git-lfs
│       ├── fused_lower.h        #   98 MB
│       ├── unfused_lower.h      #   94 MB
│       └── get_perf_stats.h
│
├── benchmarks/                  # ← NEW: one tree, config-parameterized
│   ├── Makefile                 #   make <bench> CONFIG=<cfg> FUSED=0|1
│   ├── run_sweep.sh             #   generalized generate_logs.sh
│   ├── common/
│   │   ├── fix_type_decl.py
│   │   └── small_perf_stats.h
│   ├── stats/                   #   log → CSV
│   │   ├── parse_logs.py        #   from PIM_TUNER_UTILS.py
│   │   └── to_csv.py            #   from process_dse.py
│   ├── relu/ axpy/ filter_by_key/ radix_sort/
│   ├── gemv_v{1,2,3}/ histogram/ convolution/ softmax/
│   ├── gemm_{small,medium,large}/ batched_gemm_v1/
│   └── logs/                    #   gitignored
│
├── dse/                         # ← salvaged from PiMCOM scratch dir
│   ├── dse_compile.py
│   ├── process_dse_insts.py
│   ├── generate_cost.py cost_dict.py get_eq_class_for.py compare.py
│   ├── generate_regsweep_cfg_files.py
│   ├── sweep.py                 #   was quick.py (35 KB)
│   └── cfgs/                    #   cfg_files*, cfg_reg_sweep_files, aqbolt_*
│
├── results/                     # paper CSVs
├── MISAAL/                      # submodule @ pim-fused
└── Hydride/                     # submodule @ bitserial
```

---

## 3. Benchmark consolidation

Today `MISAAL/benchmarks/bitsimd/` holds five near-duplicate trees — `Aquabolt`,
`PIMeval_Bank_LPDDR`, `PIMeval_Bank_Rank1_GDDR`, `PIMeval_Bank_Rank1_HBM`,
`PIMeval_Bank_Rank20` — differing by hardware target. These collapse into one
tree with the target selected by config file.

The existing fused/unfused switch is already env-driven
(`MISAAL_NO_FUSION=1 make <bench>`), so no source changes are needed for that
axis. `run_sweep.sh` generalizes `generate_logs.sh` to three axes:

```sh
for config in $CONFIGS; do
  for bench in $BENCHMARKS; do
    for mode in fused unfused; do
      ...
```

with outputs at `logs/<config>/<bench>_<mode>_{compute_log,data_movement_log,pim_header.h}`
so runs across targets no longer collide.

`make clean` wipes `<bench>/bin/`, so `.halide_generated.cpp` is always
regenerated and `fix_type_decl.py`'s in-place rewrite never sees patched input.

---

## 4. Stats → CSV

Each `(benchmark, mode)` pair produces two runs:

- `*_compute_log` — full run. `PIM Command Stats` gives per-command rows
  (`add.int32.h.fuse`, `mul.int32.h.fuse`, …) with
  `CNT / Runtime(ms) / Energy(mJ) / GOPS/W / %R / %W / %L`, plus a
  `TOTAL ---------` row.
- `*_data_movement_log` — same run with compute suppressed (`TOTAL` shows
  `CNT 0`), isolating host↔device transfer cost.

Hence the parser takes both files and sums them for the end-to-end number.

**Proposed CSV schema:**

| Source | Columns |
|---|---|
| filename / run | `benchmark`, `config`, `mode` (fused/unfused) |
| `PIM Params:` | `device_type`, `sim_target`, `n_cores`, `rows_per_core`, `cols_per_core`, `rank_bw_gbs`, `row_read_ns`, `row_write_ns`, `tccd_ns` |
| `Data Copy Stats:` | `h2d_bytes`, `d2h_bytes`, `d2d_bytes`, `copy_total_bytes`, `copy_runtime_ms`, `copy_energy_mj` |
| `PIM Command Stats:` TOTAL | `cmd_count`, `compute_runtime_ms`, `compute_energy_mj`, `gops_per_w`, `pct_r`, `pct_w`, `pct_l` |
| per-command rows | long-form `cmd_name`, `cnt`, `runtime_ms`, `energy_mj` |
| `PIM-Info: Fusing N command groups.` | `fused_groups` |
| derived | `total_runtime_ms`, `total_energy_mj` (compute + data movement) |

**Bugs to fix while porting:**

- `process_dse.py` derives its second input as
  `file.replace("compute_log", "data_log")`, but the benchmark driver writes
  `*_data_movement_log`. The two naming conventions disagree.
- The whole loop body sits in a bare `except: continue`, so on a mismatch it
  silently emits a CSV with missing rows instead of failing. Remove the blanket
  except; unify the log naming.
- The parser must tolerate `-nan` in the `TOTAL` row of a data-movement log.

---

## 5. Hygiene

### git-lfs

`fused_lower.h` (98 MB) and `unfused_lower.h` (94 MB) ship via LFS — the ISA is
already enumerated, so these are inputs, not build products.

Existing large blobs are already in `main`'s history (189 MB `.git`, including a
100 MB `histogram/small.bmp`, a 94 MB `input_4.bmp`, a 77 MB `.npz`). LFS
tracking only affects new commits, so realizing the benefit needs
`git lfs migrate import`, which rewrites every SHA. That happens **once, last**,
deliberately.

GitHub's free LFS tier is 1 GB storage + 1 GB/month bandwidth, and every public
clone pulls the objects. If bandwidth becomes a problem, the escape hatch is a
release-asset download in `setup.sh`.

### .gitignore

```gitignore
# egg-solver scratch
*.egg
egg.out.txt*

# run logs
*_fused_log.txt
*_unfused_log.txt
check_log check_log_fused
perf_logs/ dse_logs/ dse_logs_*/ dse_perf_logs_*/ aqbolt_dse_logs/

# per-run work dirs and their generated sources
work_dir_*/
*_work_dir_*_misaal_*.py
*_work_dir_*_misaal_*.rkt
*_test.cpp

# build products
*/bin/
*.out *.a *.o
fused_lower.cpp unfused_lower.cpp

# PLY caches
parser.out
parsetab.py
__pycache__/ *.pyc

# opentuner
opentuner.db/ *.db *.db-journal
```

### Must not ship

- `FusedOpGen/opentuner.db/miranda.cs.illinois.edu.db` — OpenTuner names its
  SQLite file after the hostname, exposing an internal UIUC machine.
- `targets/UPMEM/ISA/` — 951 JSON files scraped from the UPMEM SDK docs
  (`sdk.upmem.com/2023.2.0/201_IS.html`). Redistributing vendor instruction
  semantics is an unresolved IP question, and it is unrelated to the bitsimd
  path. Leave out unless deliberately cleared.
- `configs/asplos/LPDDR4_8Gb_x16_2400.ini` vs `LPDDR4_8GB_x16_2400.ini` —
  identical content, case-only filename difference. Breaks checkouts on
  case-insensitive filesystems. Keep only the `8GB` spelling (the one actually
  referenced).

### Hardcoded paths to parameterize

| File | Path |
|---|---|
| `FusedOpGen/PIM_API_UTILS.py:1626-1627` | `/home/arnoor2/PiMCOM/.../{fused,unfused}_perf_results.txt` |
| `FusedOpGen/PIM_TUNER_UTILS.py:278,283,292,434,439,448,451` | `/home/arnoor2/MISAAL/frontends/halide/distrib/{include,lib,tools}` |
| `perf_cost_model/generate_config_files_pim_configs.py:7` | `/home/arnoor2/pim-eval/PIMeval-PIMbench-2.0/` |
| `perf_cost_model/simple_stuck.cpp:61` | absolute `.cfg` path |
| `perf_cost_model/cfg_files/PIMeval_Bank_LPDDR.cfg:7` | dangling `.ini` path |

Also: generated `.cfg` files reference `memory_config_file =
../../../configs/asplos/<X>.ini`, resolved by DRAMsim3 against the **process
CWD**, not the cfg location — so it only works from one directory and fails
silently elsewhere. Make these explicit paths or env vars.

### Licensing

- PIM-AutoDSE is MIT (UVA). MISAAL is Apache-2.0. **PiMCOM has no LICENSE and
  no per-file headers** — needs one added before its code is vendored into an
  MIT repo. Rafae owns PiMCOM, so this is a direct decision.
- Hydride is upstream of PiMCOM's bitsimd fork; its license propagates. Akash
  owns it (co-author).
- Vendored `libpimeval.a` / `libpimeval.h` are UVA MIT code — the binaries carry
  no provenance. Attribute or rebuild from source.
- `configs/asplos/{DDR4,GDDR5,HBM2}*.ini` are byte-identical PIMeval files —
  attribute to UVA-LavaLab.

---

## 6. Phases

| # | Phase | Notes |
|---|---|---|
| 1 | Hygiene infra | `.gitignore`, `.gitattributes`, `git lfs install`, add both submodules. No file moves. |
| 2 | Vendor `isa/` | Copy PiMCOM `DRAM_BitSIMD`; drop PLY caches, opentuner DBs, UPMEM; port MISAAL's README; move lowering headers under LFS. |
| 3 | Benchmarks | Largest piece. Diff the five `PIMeval_Bank_*` trees to confirm config-only deltas, then parameterize. |
| 4 | Stats + DSE utils | Port `PIM_TUNER_UTILS` parser, wire CSV into the Makefile, salvage the ~10 DSE scripts. |
| 5 | Build wiring | `setup.sh`: submodules → Hydride → Halide (pinned LLVM) → `libpimeval` → smoke-run one benchmark both modes. |
| 6 | Docs | README, CITATION, and a paper-figure → (benchmark, config) reproduction table. |
| 7 | History rewrite | `git lfs migrate import`, curate history. Destructive, rewrites every SHA. Last, with explicit go-ahead. |

---

## 7. Resolved

1. **The five `PIMeval_Bank_*` trees differ only by config.** Confirmed by
   diffing all five Makefiles: the only deltas are `CSV_NAME` (the per-target
   cost-model CSV), the benchmark list (`batched_gemm_v1` only in LPDDR,
   `softmax` absent from Aquabolt, Aquabolt omits `test/stubs.cpp`), and
   whitespace. Consolidation is safe.
2. **LLVM 12.** `distrib/halide_config.make` records `-DLLVM_VERSION=120`.
   Halide's Makefile accepts 12-15; `setup.sh` warns outside that range.
3. **The hardware config is selected at run time**, not build time - it was
   hardcoded at `test/run.cpp:126`. Now read from `$PIM_CONFIG`, so one build
   of the tree serves every target.
4. **Rosette/Racket are provenance-only.** They were needed for the original
   synthesis of the rewrite rules; the release starts from the already
   synthesized rules, so the default flow does not need them.

## 8. Open questions

1. **The rewrite-rule pickles ship via Git LFS** (decided). `lib/patterns/` in
   MISAAL is 2.5 GB and only 6 `.py` files are tracked - every pickle is
   untracked, so a fresh submodule clone cannot run. `PIM.py` loads the
   `*_v2_fix` trio (256 MB + 91 MB + 91 MB); `.gitattributes` declaring LFS
   for exactly those three is written into the MISAAL checkout.

   **Still to do, and it cannot be done from here:** `git-lfs` is not installed
   on this machine, so the objects cannot actually be converted. Once it is:

   ```sh
   cd MISAAL && git lfs install
   git add .gitattributes lib/patterns/bitserial_fused_v2_fix.pickle \
           lib/patterns/bitserial_fused_abstract_v2_fix.pickle \
           lib/patterns/bitserial_fused_abstract_simplified_v2_fix.pickle
   git lfs ls-files      # confirm they are pointers, not raw blobs
   git commit -m "Track bitserial rewrite rules in LFS"
   ```

   Not committed here: MISAAL is a separate repo whose tree is dirty (54
   modified, 31 deleted, 795 untracked), so staging there is a deliberate act.
   The remaining ~2 GB of `_scaled` / `_self` / non-v2 pickles are superseded
   and should not ship. Note ~438 MB of pickles plus ~190 MB of lowering
   headers is ~62% of GitHub's 1 GB free LFS tier, and bandwidth is per-clone.
2. **Does MISAAL's stale `targets/pim_fused/codegen-generator/` get removed from
   `pim-fused`?** With both submodules present, a checkout otherwise contains
   two copies of the bitsimd codegen and someone will run the older one.
3. **`libpimeval.a` is redistributed as a prebuilt binary in 8 places** across
   MISAAL, with no accompanying LICENSE - and `libpimeval.h` explicitly points
   at one ("See the LICENSE file in the root of this repository"). That is the
   clearest license-compliance defect in the tree. Recommend dropping the
   archives and declaring PIMeval-PIMbench an external pinned dependency.
4. **Two benchmarks have incomplete runs in the LPDDR sample**:
   `radix_sort_fused` produced no `PIM Command Stats` block, and
   `filter_by_key` has no fused compute log at all. Expected gaps, or failures
   worth re-running?
