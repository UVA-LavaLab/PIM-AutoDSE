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

## These files are not yet in the repository

They exceed GitHub's 100 MB per-file limit for ordinary blobs and must be
tracked with Git LFS. `.gitattributes` at the repository root already declares
the rules:

```
isa/lowering/fused_lower.h    filter=lfs diff=lfs merge=lfs -text
isa/lowering/unfused_lower.h  filter=lfs diff=lfs merge=lfs -text
isa/lowering/get_perf_stats.h filter=lfs diff=lfs merge=lfs -text
```

To populate the directory:

```sh
# once per machine
sudo apt-get install git-lfs   # or: brew install git-lfs
git lfs install

# from the repository root
cp /path/to/fused_lower.h  isa/lowering/
cp /path/to/unfused_lower.h isa/lowering/
cp /path/to/get_perf_stats.h isa/lowering/
git add isa/lowering/*.h
git commit -m "Add generated lowering interfaces (LFS)"
```

Verify they were stored as LFS pointers rather than raw blobs before pushing:

```sh
git lfs ls-files
```

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
