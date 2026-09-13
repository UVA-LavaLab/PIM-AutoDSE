# Lowering interfaces

This directory holds the two generated C++ interfaces the benchmarks compile
against:

| File | Size | Role |
|---|---|---|
| `fused_lower.h` | ~98 MB | Each CISC instruction submitted as one `PimProg` closed with `pimFuse` — intermediates stay inside the PIM unit |
| `unfused_lower.h` | ~94 MB | The same instructions without fusion; every intermediate spills to the DRAM array, costing an ACTIVATE/PRECHARGE pair |
| `get_perf_stats.h` | ~1.6 MB | Stats-collection helpers shared by both |

The benchmark build picks one via `FUSED=1` (fused) or `FUSED=0` (unfused).
Because both are generated from the same enumeration and linked against the
same freshly built `libpimeval.a`, the runtime and energy delta between them is
attributable to fusion alone.

## Git LFS

These files are tracked with Git LFS (rules in the root `.gitattributes`).
Install it before cloning, or the headers check out as ~130-byte pointer files
and the build fails:

```sh
sudo apt-get install git-lfs   # or: brew install git-lfs / conda install -c conda-forge git-lfs
git lfs install
```

If you already cloned without it, run `git lfs pull` from the repository root.

## Compatibility typedefs

Each header begins with two aliases:

```c++
using PimProg    = PimFusionBlock;
using PimProgApi = PimApi;
```

The headers were generated when libpimeval named its fusion-program types
`PimProg` / `PimProgApi`; the libpimeval in this repository renamed them. The
structs are otherwise identical, so the aliases are exact. Regenerated headers
should use the current names instead.

## Bandwidth note

GitHub's free LFS tier is 1 GB storage and 1 GB/month bandwidth, and **every
clone pulls the objects**. These three files are ~190 MB, so a handful of
clones per month exhausts the free bandwidth. If that becomes a problem, the
usual alternative is to publish them as a GitHub release asset (or archive them
with a DOI) and have `setup.sh` fetch them — which also keeps them out of the
git history entirely.

## Regenerating

They are reproducible from the enumeration rather than being primary sources.
See [`../README.md`](../README.md) — but note that regenerating requires
Racket/Rosette and the Hydride submodule, and the shipped release deliberately
starts from the already-generated artifacts.
