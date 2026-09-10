# ===========================================================================
# PIM-AutoDSE benchmarks
#
# One benchmark tree, retargeted by config file rather than by copying the
# tree per hardware target.
#
#   make <bench> CONFIG=<name> FUSED=<0|1>
#
#   CONFIG   hardware target, a basename in configs/   (default: PIMeval_Bank_LPDDR)
#   FUSED    1 = fused ISA lowering, 0 = unfused        (default: 1)
#
# Examples:
#   make axpy                                   # LPDDR, fused
#   make axpy CONFIG=PIMeval_Bank_Rank20 FUSED=0
#   make all  CONFIG=Aquabolt
#
# Requires `source env.sh` first (HALIDE_DISTRIB, LLVM_ROOT, PIM_*).
# ===========================================================================

# --- Toolchain (from env.sh; overridable) ----------------------------------
HALIDE_DISTRIB ?= $(error HALIDE_DISTRIB is not set - run: source ../env.sh)

# Use conditionals rather than `?=` for the fallbacks below: `?=` only assigns
# when a variable is *undefined*, so an exported-but-empty HALIDE_DIR (easy to
# produce by accident) would stay empty and the generator would fail at run
# time with "libHalide.so: cannot open shared object file".
ifeq ($(strip $(HALIDE_DIR)),)
  HALIDE_DIR := $(HALIDE_DISTRIB)
endif
ifeq ($(strip $(LLVM_ROOT)),)
  LLVM_ROOT := $(LLVM_DIS_ROOT)
endif
ifeq ($(strip $(LLVM_ROOT)),)
  $(error LLVM_ROOT/LLVM_DIS_ROOT is not set - run: source ../env.sh)
endif
# NOTE: use CXX, not CC. `CC` is a make built-in that defaults to `cc`, so
# `CC ?= g++` never takes effect and the C compiler gets used for C++ -
# which fails at link time with "DSO missing from command line" on libstdc++.
CXX            ?= g++

BENCH_ROOT     := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
CONFIG_DIR     := $(BENCH_ROOT)/configs
COMMON_DIR     := $(BENCH_ROOT)/common
LOG_ROOT       := $(BENCH_ROOT)/logs

# --- Target selection ------------------------------------------------------
CONFIG   ?= PIMeval_Bank_LPDDR
FUSED    ?= 1
CFG_FILE := $(CONFIG_DIR)/$(CONFIG).cfg

ifeq ($(FUSED),1)
  MODE            := fused
  FUSION_DEFINE   := -DFUSED
  MISAAL_NO_FUSION := 0
else
  MODE            := unfused
  FUSION_DEFINE   := -DUNFUSED
  MISAAL_NO_FUSION := 1
endif

LOG_DIR := $(LOG_ROOT)/$(CONFIG)

# Cost model CSV consumed by the MISAAL generator. Per-target; optional.
VF            ?= 65536
COST_CSV_ROOT ?= $(PIM_ISA_ROOT)/perf_cost_model/perf_logs
COST_CSV      ?= $(COST_CSV_ROOT)/pim_perf_results_config_$(CONFIG)_vf$(VF).csv

# --- Codegen knobs ---------------------------------------------------------
ENABLE_HYDRIDE ?= 1
EXPR_DEPTH     ?= 2
SYNTH_BITWIDTH ?= 16
STARTING_HASH  ?= "empty_hash"
TARGET         ?= host

benchmarks = tensor_add \
             axpy \
             relu \
             gemv_v1 \
             gemv_v2 \
             gemv_v3 \
             gemm_small \
             gemm_medium \
             gemm_large \
             batched_gemm_v1 \
             bitsimd_gemv \
             histogram \
             filter_by_key \
             convolution \
             max_pool \
             radix_sort \
             radix_sort_i16 \
             softmax

bin_dirs = $(addsuffix /bin,$(benchmarks))
hannk    = hannk/common_halide.cpp

# NOTE: ./libpimsim/decls comes BEFORE ./libpimsim on the include path.
# The Halide-generated source does `#include "fused_lower.h"`, and that header
# is 94 MB / 3.19M lines of function *definitions* - compiling it into every
# benchmark TU took >13 minutes each, for a benchmark that calls one function.
# libpimsim/decls/ holds a declarations-only version (9 MB), and the
# definitions are compiled once into libpimsim/{fused,unfused}_lib.o.
CXXFLAGS_COMMON = -DHALIDE_CPP_ALWAYS_USE_CPP_VECTORS --std=c++17 \
                  -O3 -march=native -mavx512vl -mavx512ifma \
                  -ffunction-sections -fdata-sections \
                  -I $(HALIDE_DISTRIB)/include \
                  -I ./libpimsim/decls -I ./libpimsim/ \
                  -lstdc++ -ldl -pthread

# The prebuilt lowering object matching the selected mode. --gc-sections drops
# the 42505 ops this benchmark does not call.
LOWER_LIB := ./libpimsim/$(MODE)_lib.o

LDFLAGS_COMMON  = -L ./libpimsim/ -lpimeval -Wl,--gc-sections

# NOTE: test/stubs.cpp is deliberately NOT linked.
#
# It no-ops the pthread API to force single-threaded simulation, but it does
# not stub pthread_create - so libpimeval still spins up its thread pool while
# every mutex, condvar and TLS call becomes a no-op, which is a data race.
# Its pthread_once also returns without running the initialiser, which leaves
# libstdc++'s locale globals unconstructed; the first std::ifstream (reached
# from pimCreateDeviceFromConfig via pimUtils::readFileContent) then segfaults
# inside std::ctype<char>::ctype. printf-based output hides this, streams do
# not - which is why switching libpimeval from std::cout to printf appeared to
# fix it.
#
# For single-threaded determinism use the supported knob instead:
#     max_num_threads = 1        in the .cfg, or
#     PIMEVAL_MAX_NUM_THREADS=1  in the environment


.PHONY: clean all list check-env $(benchmarks)

all: $(benchmarks)

list:
	@echo "Benchmarks:"; printf '  %s\n' $(benchmarks)
	@echo "Configs:";    ls $(CONFIG_DIR)/*.cfg | xargs -n1 basename | sed 's/\.cfg$$//' | sed 's/^/  /'

check-env:
	@test -n "$(HALIDE_DISTRIB)" || { echo "HALIDE_DISTRIB unset - run: source ../env.sh"; exit 1; }
	@test -f "$(CFG_FILE)" || { echo "no such config: $(CFG_FILE)"; echo "try: make list"; exit 1; }
	@test -f "$(COST_CSV)" || echo "[warn] cost model CSV not found: $(COST_CSV)"
	@test -f "$(LOWER_LIB)" || { echo "missing $(LOWER_LIB) - run ../setup.sh to build the lowering objects"; exit 1; }

# ---------------------------------------------------------------------------
# Build + run one benchmark for the selected (CONFIG, MODE)
# ---------------------------------------------------------------------------
$(benchmarks): %: %/bin check-env
	@echo "==> $@  [config=$(CONFIG) mode=$(MODE)]"

#	1. Build the Halide generator
	$(CXX) --std=c++17 -fno-rtti -O3 -DLOG2VLEN=7 \
		-I $(HALIDE_DISTRIB)/include -I $(HALIDE_DISTRIB)/tools \
		-g $@/src/$@_generator.cpp $(HALIDE_DISTRIB)/tools/GenGen.cpp \
		$(hannk) \
		-o $@/bin/$@_generator \
		-L $(HALIDE_DISTRIB)/lib -lHalide \
		$(shell $(LLVM_ROOT)/bin/llvm-config --system-libs --link-static)

#	2. Run the generator -> MISAAL/PIM lowering
	export LD_LIBRARY_PATH=$(HALIDE_DIR)/lib; \
	HL_EXPR_DEPTH=$(EXPR_DEPTH) \
	HYDRIDE_BENCHMARK=$@_misaal \
	HL_ENABLE_MISAAL=$(ENABLE_HYDRIDE) \
	MISAAL_EQ_SAT_ITERS=5 \
	HL_ENABLE_HYDRIDE=$(ENABLE_HYDRIDE) \
	PIM_HEADER_FILE=$@_pim_header.h \
	HL_SYNTH_BW=$(SYNTH_BITWIDTH) \
	HYDRIDE_INITIAL_HASH=$(STARTING_HASH) \
	MISAAL_DISABLE_FRONTEND_PATTERNS=1 \
	COST_FILE_CSV_NAME=$(COST_CSV) \
		./$@/bin/$@_generator -t 0 -o $@/bin -g $@ -e cpp,h,stmt -f $@ target=$(TARGET)

#	3. Halide runtime object
	export LD_LIBRARY_PATH=$(HALIDE_DIR)/lib; \
	HL_EXPR_DEPTH=$(EXPR_DEPTH) HL_ENABLE_HYDRIDE=0 HL_DEBUG_CODEGEN=1 \
	HL_SYNTH_BW=$(SYNTH_BITWIDTH) MISAAL_DISABLE_FRONTEND_PATTERNS=1 \
		./$@/bin/$@_generator -r halide_runtime_x86 -o $@/bin -e object,c_header target=$(TARGET)

ifeq ($(ENABLE_HYDRIDE),1)
#	4. Repair the generated C++ (REQUIRED - Halide emits misaal_* definitions
#	   with wrong parameter types; fix_type_decl.py recovers them from the
#	   forward declarations). inline.py must run first.
	python3 $(COMMON_DIR)/inline.py $@ $@_pim_header.h
	python3 $(COMMON_DIR)/fix_type_decl.py $@

#	5. Compute-profiling binary (fused or unfused lowering)
	$(CXX) -DPROFILE_COMPUTE=1 $(FUSION_DEFINE) -Dbenchmark_$@ \
		$(CXXFLAGS_COMMON) -I ./$@/bin \
		test/run.cpp $@/bin/$@.halide_generated.cpp \
		$(LOWER_LIB) $(LDFLAGS_COMMON) $@/bin/halide_runtime_x86.o \
		-o $@/bin/$@_compute_run.out

#	6. Data-movement-profiling binary (compute suppressed)
	$(CXX) -DPROFILE_DATA_MOVEMENT=1 -Dbenchmark_$@ \
		$(CXXFLAGS_COMMON) -I ./$@/bin \
		test/run.cpp $@/bin/$@.halide_generated.cpp \
		$(LOWER_LIB) $(LDFLAGS_COMMON) $@/bin/halide_runtime_x86.o \
		-o $@/bin/$@_data_movement_run.out

#	7. Run both under the selected config
	@mkdir -p $(LOG_DIR)
	PIM_CONFIG=$(CFG_FILE) LOG_DIR=$(LOG_DIR) MODE=$(MODE) \
		bash $(COMMON_DIR)/profile_compute_and_data.sh $@
endif

ifeq ($(ENABLE_HYDRIDE),0)
	$(CXX) -DPROFILE_COMPUTE=1 -Dbenchmark_$@ \
		$(CXXFLAGS_COMMON) -I ./$@/bin \
		test/run.cpp $@/bin/$@.halide_generated.cpp \
		$(LOWER_LIB) $(LDFLAGS_COMMON) $@/bin/halide_runtime_x86.o \
		-o $@/bin/$@_compute_run.out
endif

$(bin_dirs): %:
	mkdir -p $@

clean:
	rm -rf $(bin_dirs)
	rm -rf halide_expr_*
	rm -rf /tmp/hydride_exprs.rkt /tmp/base_* /tmp/hydride_hash* /tmp/hydride.node*
	rm -rf ./*misaal.py ./*.egg ./*.txt ./*.rkt ./test.out ./egg*
	rm -rf ./pim_fused_lowering.h
	rm -rf ./*_pim_header.h ./*_pim_header.h.defs
	rm -rf ./*_log

distclean: clean
	rm -rf $(LOG_ROOT) results
