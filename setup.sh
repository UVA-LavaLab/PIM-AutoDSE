#!/usr/bin/env bash
#
# PIM-AutoDSE setup
# =================
#
# Brings up a complete build:
#
#   1. Initialise submodules   MISAAL @ pim-fused, Hydride @ bitserial,
#                              egglog @ 14542d7
#   2. Build Halide            MISAAL/frontends/halide -> distrib/
#   3. Build libpimeval        the PIM simulator
#   4. Emit env.sh             environment for the benchmark flow
#
# Usage:
#     ./setup.sh                  # full setup
#     ./setup.sh --skip-halide    # Halide distrib already built
#     ./setup.sh --env-only       # just (re)write env.sh
#
# Halide requires LLVM 12-15. Point LLVM_CONFIG at your llvm-config binary:
#     LLVM_CONFIG=/path/to/llvm-12/bin/llvm-config ./setup.sh
#
set -euo pipefail

PIM_AUTODSE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

SKIP_HALIDE=0
SKIP_SIM=0
ENV_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --skip-halide) SKIP_HALIDE=1 ;;
        --skip-sim)    SKIP_SIM=1 ;;
        --env-only)    ENV_ONLY=1 ;;
        -h|--help)     sed -n '2,25p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# 0. Prerequisites
# ---------------------------------------------------------------------------
check_prereqs() {
    log "Checking prerequisites"

    local missing=()
    for tool in git g++ make python3; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    [ ${#missing[@]} -eq 0 ] || die "missing required tools: ${missing[*]}"

    if ! command -v git-lfs >/dev/null 2>&1; then
        warn "git-lfs is not installed."
        warn "  The ISA lowering headers (isa/lowering/*.h, ~190 MB) are stored"
        warn "  in LFS and will be checked out as pointer stubs without it."
        warn "  Install:  apt-get install git-lfs   (then: git lfs install)"
    else
        git lfs install --local >/dev/null 2>&1 || true
    fi

    python3 - <<'PY' || warn "python dependencies missing; run: pip install -r requirements.txt"
import importlib, sys
missing = [m for m in ("pandas", "toml", "ply") if not importlib.util.find_spec(m)]
sys.exit(1 if missing else 0)
PY
}

# ---------------------------------------------------------------------------
# 1. Submodules
# ---------------------------------------------------------------------------
init_submodules() {
    log "Initialising submodules"
    git -C "$PIM_AUTODSE_ROOT" submodule update --init --recursive

    for sub in MISAAL Hydride egglog; do
        [ -d "$PIM_AUTODSE_ROOT/$sub/.git" ] || [ -f "$PIM_AUTODSE_ROOT/$sub/.git" ] \
            || die "submodule $sub failed to initialise"
    done

    log "  MISAAL  @ $(git -C "$PIM_AUTODSE_ROOT/MISAAL"  rev-parse --short HEAD)"
    log "  Hydride @ $(git -C "$PIM_AUTODSE_ROOT/Hydride" rev-parse --short HEAD)"
    log "  egglog  @ $(git -C "$PIM_AUTODSE_ROOT/egglog"  rev-parse --short HEAD)"
}

# ---------------------------------------------------------------------------
# 2. Halide
#
# MISAAL's Halide fork carries the PIM/MISAAL codegen path. Its Makefile
# resolves LLVM through $LLVM_CONFIG.
#
# IMPORTANT: build via `make distrib`, NOT CMake. Halide ships both build
# systems, but the MISAAL fork's PIM codegen path is only wired into the
# Makefile - a CMake build produces a libHalide without it. Do not "modernise"
# this to cmake/ninja.
#
# Building LLVM itself is a multi-hour job and deliberately NOT done here -
# point LLVM_CONFIG at an existing LLVM 12-15 install instead.
# ---------------------------------------------------------------------------
find_llvm() {
    if [ -n "${LLVM_CONFIG:-}" ]; then
        [ -x "$LLVM_CONFIG" ] || die "LLVM_CONFIG is set but not executable: $LLVM_CONFIG"
        echo "$LLVM_CONFIG"; return
    fi

    # Halide's own default location, then anything on PATH.
    local candidates=("$PIM_AUTODSE_ROOT/MISAAL/frontends/halide/llvm-build/bin/llvm-config")
    for v in 15 14 13 12; do
        candidates+=("$(command -v "llvm-config-$v" 2>/dev/null || true)")
    done
    candidates+=("$(command -v llvm-config 2>/dev/null || true)")

    for c in "${candidates[@]}"; do
        [ -n "$c" ] && [ -x "$c" ] && { echo "$c"; return; }
    done
    return 1
}

build_halide() {
    local halide_src="$PIM_AUTODSE_ROOT/MISAAL/frontends/halide"
    [ -d "$halide_src" ] || die "Halide source not found at $halide_src (did submodules init?)"

    if [ -f "$halide_src/distrib/lib/libHalide.so" ] && [ "$SKIP_HALIDE" -eq 1 ]; then
        log "Halide distrib already present - skipping (--skip-halide)"
        return
    fi

    local llvm_config
    llvm_config="$(find_llvm)" || die "$(cat <<'EOF'
No llvm-config found.

Halide requires LLVM 12-15 (the reference build for this artifact used LLVM 12).
Install one and re-run, pointing LLVM_CONFIG at it:

    LLVM_CONFIG=/path/to/llvm-12/bin/llvm-config ./setup.sh

On Debian/Ubuntu:  apt-get install llvm-14-dev libclang-14-dev clang-14
EOF
)"

    local llvm_version
    llvm_version="$("$llvm_config" --version)"
    log "Building Halide against LLVM $llvm_version ($llvm_config)"

    case "$llvm_version" in
        1[2-5].*) ;;
        *) warn "LLVM $llvm_version is outside the tested 12-15 range; build may fail" ;;
    esac

    # Halide's Makefile falls back to $(HYDRIDE_ROOT)/frontends/halide/llvm-build
    # for llvm-config; we pass LLVM_CONFIG explicitly so that default is unused,
    # but HYDRIDE_ROOT still names the Hydride checkout.
    #
    # Build the library target, NOT `distrib`. `distrib` is
    #     distrib: $(DISTRIB_DIR)/lib/libHalide.$(SHARED_EXT) autoschedulers
    # and the adams2019 autoscheduler does not build in this fork: its
    # cost_model.generator runs the MISAAL codegen path ("MIS enabled") and
    # emits a struct initializer that an assertions-enabled LLVM 12 rejects:
    #     Assertion `V[I]->getType() == ST->getTypeAtIndex(I) &&
    #                "Initializer for struct element doesn't match!"' failed.
    # The autoschedulers are stock Halide features and are not used by the PIM
    # benchmark flow, which needs libHalide plus the headers and tools.
    ( cd "$halide_src" \
        && HYDRIDE_ROOT="$PIM_AUTODSE_ROOT/Hydride" \
           LLVM_CONFIG="$llvm_config" \
           make -j"$JOBS" distrib/lib/libHalide.so )

    [ -f "$halide_src/distrib/lib/libHalide.so" ] \
        || die "Halide build finished but distrib/lib/libHalide.so is missing"
    for needed in include/Halide.h tools/GenGen.cpp; do
        [ -e "$halide_src/distrib/$needed" ] \
            || die "Halide distrib incomplete: missing $needed"
    done
    log "Halide built at $halide_src/distrib"
}

# ---------------------------------------------------------------------------
# 2b. egglog (equality saturation)
#
# Pinned to 14542d7 - MISAAL's rewrite driver is sensitive to egglog's output
# format, and this is the revision the artifact was developed against.
# ---------------------------------------------------------------------------
build_egglog() {
    local egg_src="$PIM_AUTODSE_ROOT/egglog"
    [ -d "$egg_src" ] || die "egglog not found at $egg_src (did submodules init?)"

    if ! command -v cargo >/dev/null 2>&1; then
        warn "cargo not found - skipping egglog build."
        warn "  Install Rust (https://rustup.rs) and re-run, or build manually:"
        warn "    cd egglog && cargo build --release"
        return
    fi

    if [ -x "$egg_src/target/release/egglog" ]; then
        log "egglog already built"
        return
    fi

    log "Building egglog (release)"
    ( cd "$egg_src" && cargo build --release )
    [ -x "$egg_src/target/release/egglog" ] || die "egglog build produced no binary"
    log "egglog built at $egg_src/target/release/egglog"
}

# ---------------------------------------------------------------------------
# 3. libpimeval (the simulator)
# ---------------------------------------------------------------------------
build_libpimeval() {
    log "Building libpimeval"

    # Pass the `perf` goal EXPLICITLY. libpimeval's Makefile gates its
    # optimisation flags on $(MAKECMDGOALS):
    #     ifeq ($(MAKECMDGOALS),perf)
    #         CXXFLAGS += $(CXXFLAGS_PERF)   # -O3
    # With a bare `make`, MAKECMDGOALS is empty, so .DEFAULT_GOAL still builds
    # the `perf` target but the -O3 flags are never added - you get an
    # unoptimised simulator that silently runs far slower.
    ( cd "$PIM_AUTODSE_ROOT" && make -j"$JOBS" -C libpimeval perf )

    # Symlink libpimeval.h into libpimeval/include/
    ( cd "$PIM_AUTODSE_ROOT" && make -C libpimeval create_link >/dev/null )

    local lib="$PIM_AUTODSE_ROOT/libpimeval/lib/libpimeval.a"
    [ -f "$lib" ] || die "libpimeval.a not produced at $lib"
    log "libpimeval built at $lib"
}

# ---------------------------------------------------------------------------
# 3b. benchmarks/libpimsim - what the benchmark Makefile links against
#
# The Makefile compiles with `-I ./libpimsim/ -L ./libpimsim/ -lpimeval`, so
# this directory has to hold the simulator archive, its header, and the ISA
# lowering headers. Upstream this was a per-target copy of all of it; here it
# is symlinks to the single built artifacts.
# ---------------------------------------------------------------------------
stage_libpimsim() {
    local dst="$PIM_AUTODSE_ROOT/benchmarks/libpimsim"
    log "Staging $dst"
    mkdir -p "$dst"

    ln_force() {
        local target="$1" name="$2"
        [ -e "$target" ] || return 1
        ln -sfn "$target" "$dst/$name"
    }

    ln_force "$PIM_AUTODSE_ROOT/libpimeval/lib/libpimeval.a" libpimeval.a \
        || die "libpimeval.a missing - did the simulator build?"
    ln_force "$PIM_AUTODSE_ROOT/libpimeval/src/libpimeval.h" libpimeval.h \
        || die "libpimeval.h missing"

    local missing=()
    for header in fused_lower.h unfused_lower.h get_perf_stats.h; do
        ln_force "$PIM_AUTODSE_ROOT/isa/lowering/$header" "$header" \
            || missing+=("$header")
    done

    if [ ${#missing[@]} -ne 0 ]; then
        warn "ISA lowering headers not present: ${missing[*]}"
        warn "  They live in isa/lowering/ and are tracked with Git LFS."
        warn "  Without them the benchmarks will not compile."
        warn "  See isa/lowering/README.md."
    fi
}

# ---------------------------------------------------------------------------
# 4. env.sh
# ---------------------------------------------------------------------------
write_env() {
    log "Writing env.sh"

    local llvm_config
    llvm_config="$(find_llvm 2>/dev/null || echo '')"

    cat > "$PIM_AUTODSE_ROOT/env.sh" <<EOF
# Generated by setup.sh on $(date -u +%Y-%m-%dT%H:%M:%SZ) - do not edit by hand.
# Source before building or running benchmarks:  source env.sh
#
# Mirrors the original hand-rolled sequence (Hydride -> MISAAL -> bitsimd ISA),
# with all absolute paths made relative to this checkout.

export PIM_AUTODSE_ROOT="$PIM_AUTODSE_ROOT"

# ---------------------------------------------------------------------------
# LLVM  (Halide requires 12-15; the reference build used LLVM 12)
# ---------------------------------------------------------------------------
export LLVM_CONFIG="${llvm_config}"
export LLVM_ROOT="\$(dirname "\$(dirname "\$LLVM_CONFIG")")"
export LLVM_DIS_ROOT="\$LLVM_ROOT"
export LLVM_DIS="\$LLVM_ROOT/bin/llvm-dis"
export PATH="\$LLVM_ROOT/bin:\$PATH"

# ---------------------------------------------------------------------------
# Hydride  (Rose / Rosette IR framework)
# ---------------------------------------------------------------------------
export HYDRIDE_ROOT="\$PIM_AUTODSE_ROOT/Hydride"
export HYDRIDE_SRC="\$HYDRIDE_ROOT"

_CG="\$HYDRIDE_ROOT/codegen-generator"
export INTRINSICS_LL="\$_CG/tools/low-level-codegen/InstSelectors/bitsimd/bitsimd_wrappers.ll"

# Prebuilt legalizer shared object. The upstream scripts hardcoded
# /shared/hydride/LLVMBitSIMDLegalizer.so, which does not exist outside the
# original machine - override LEGALIZER_PATH if you have built your own.
export LEGALIZER_PATH="\${LEGALIZER_PATH:-\$_CG/tools/low-level-codegen/build/LLVMBitSIMDLegalizer.so}"

export PATH="\$HYDRIDE_ROOT/rosette/bin:\$HYDRIDE_ROOT/bin:\$PATH"
export LD_LIBRARY_PATH="\$_CG/tools/low-level-codegen/build:\${LD_LIBRARY_PATH:-}"

# ---------------------------------------------------------------------------
# MISAAL / Halide   (sourced after Hydride: HALIDE_* intentionally override)
# ---------------------------------------------------------------------------
export MISAAL_SRC="\$PIM_AUTODSE_ROOT/MISAAL"
export HALIDE_SRC="\$MISAAL_SRC/frontends/halide"
export HALIDE_DISTRIB="\$HALIDE_SRC/distrib"
export HALIDE_DIR="\$HALIDE_DISTRIB"
export LD_LIBRARY_PATH="\$HALIDE_DISTRIB/lib:\$LD_LIBRARY_PATH"

# ---------------------------------------------------------------------------
# Simulator + bitsimd ISA
# ---------------------------------------------------------------------------
export PIM_EVAL_ROOT="\$PIM_AUTODSE_ROOT/libpimeval"
export PIM_ISA_ROOT="\$PIM_AUTODSE_ROOT/isa"
export PIM_LOWERING_DIR="\$PIM_ISA_ROOT/lowering"

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
export PIM_BENCH_ROOT="\$PIM_AUTODSE_ROOT/benchmarks"
export PIM_CONFIG_DIR="\$PIM_BENCH_ROOT/configs"
# Default hardware target; override per run with CONFIG=<name>
export PIM_CONFIG="\${PIM_CONFIG:-\$PIM_CONFIG_DIR/PIMeval_Bank_LPDDR.cfg}"

# ---------------------------------------------------------------------------
# egglog (equality saturation)
#
# NOTE: MISAAL reads this path from lib/utils/egg_config.py, which currently
# hardcodes an absolute path. Until that reads $EGG_PKG_PATH, point it here.
# ---------------------------------------------------------------------------
export EGG_PKG_PATH="\$PIM_AUTODSE_ROOT/egglog"
export PATH="\$EGG_PKG_PATH/target/release:\$EGG_PKG_PATH/target/debug:\$PATH"

# ---------------------------------------------------------------------------
# External tools
# ---------------------------------------------------------------------------
# Racket is required by Rosette (synthesis). Set RACKET_ROOT if not on PATH.
[ -n "\${RACKET_ROOT:-}" ] && export PATH="\$RACKET_ROOT/bin:\$PATH"

# ---------------------------------------------------------------------------
# PYTHONPATH  (Hydride codegen-generator, MISAAL lib, bitsimd ISA)
# ---------------------------------------------------------------------------
export PYTHONPATH="\$_CG/rosette-ir:\${PYTHONPATH:-}"
export PYTHONPATH="\$_CG/codegen/rosette:\$_CG/codegen/llvm:\$PYTHONPATH"
export PYTHONPATH="\$_CG/analysis:\$_CG/transform:\$PYTHONPATH"
export PYTHONPATH="\$_CG/tools:\$_CG/tools/code-generator:\$PYTHONPATH"
export PYTHONPATH="\$_CG/tools/low-level-codegen:\$_CG/tools/rosette-lifter:\$PYTHONPATH"
export PYTHONPATH="\$_CG/tools/similarity-checker:\$_CG/tools/validity-checker:\$PYTHONPATH"
export PYTHONPATH="\$_CG/tools/transformations-verifier:\$_CG/tools/llvmlite:\$PYTHONPATH"
export PYTHONPATH="\$_CG/tools/fuzzer:\$PYTHONPATH"
# All four target dirs are needed, not just bitsimd: the synthesizer
# (code-synthesizer/dsl-ir/synthesizer/SynthesizerBase.py) imports
# ARMLegalInst and friends unconditionally.
export PYTHONPATH="\$_CG/targets/bitsimd:\$_CG/targets/arm:\$_CG/targets/x86:\$_CG/targets/hexagon:\$PYTHONPATH"
export PYTHONPATH="\$HYDRIDE_ROOT/code-synthesizer/dsl-ir:\$PYTHONPATH"
export PYTHONPATH="\$MISAAL_SRC/lib:\$PYTHONPATH"
export PYTHONPATH="\$PIM_ISA_ROOT/parser:\$PIM_ISA_ROOT/gen:\$PIM_ISA_ROOT/perf_cost_model:\$PYTHONPATH"

unset _CG
EOF

    log "env.sh written - run: source env.sh"
}

# ---------------------------------------------------------------------------
main() {
    log "PIM-AutoDSE setup (root: $PIM_AUTODSE_ROOT)"

    if [ "$ENV_ONLY" -eq 1 ]; then
        write_env
        exit 0
    fi

    check_prereqs
    init_submodules
    [ "$SKIP_HALIDE" -eq 1 ] || build_halide
    build_egglog
    [ "$SKIP_SIM"    -eq 1 ] || build_libpimeval
    stage_libpimsim
    write_env

    cat <<EOF

$(printf '\033[1;32m==> Setup complete\033[0m')

Next:
    source env.sh
    cd benchmarks
    python3 run_benchmarks.py --list                    # what is available
    python3 run_benchmarks.py --benchmark axpy          # one benchmark, all targets
    python3 run_benchmarks.py --all --csv results.csv   # full sweep -> CSV

EOF
}

main "$@"
