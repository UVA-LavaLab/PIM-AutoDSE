from PIM_API_UTILS import PimDeviceEnum, PIM_CONFIG, get_device_type_name
from GenPimFusedCost import evaluate_config, get_csv_name
import subprocess as sb

from utils.DSLInstructionUtils import get_random_tempfile_name

import os

# Resolved from the environment (see env.sh at the repo root) rather than
# hardcoded to one machine's checkout.
HALIDE_DISTRIB = os.environ.get("HALIDE_DISTRIB")
if HALIDE_DISTRIB is None:
    raise RuntimeError(
        "HALIDE_DISTRIB is not set - run `source env.sh` at the repo root")
HALIDE_DISTRIB = HALIDE_DISTRIB.rstrip("/")


# Benchmark tree the DSE flow builds from. Paths are absolute so the flow can
# be driven from any directory (dse_compile.py runs from dse/).
_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BENCH_ROOT = os.environ.get("PIM_BENCH_ROOT") or os.path.join(_REPO_ROOT, "benchmarks")
LIBPIMSIM = os.path.join(BENCH_ROOT, "libpimsim")
ENSURE_COST_MODEL = os.path.join(BENCH_ROOT, "common", "ensure_cost_model.py")
COST_CSV_ROOT = os.path.join(_REPO_ROOT, "isa", "perf_cost_model", "perf_logs")

def execute_cmd(cmd, env = None):
    print(f"$\t{cmd}")
    result = sb.run(cmd, shell = True, stdout = sb.PIPE, stderr = sb.STDOUT, env = env)
    if result.returncode != 0:
        output = result.stdout.decode(errors = "replace").splitlines()
        print(f"Command failed (exit {result.returncode}):", cmd)
        print("\n".join(output[-20:]))
        return False
    return True


def get_result_stats_from_pim_log_file(fname):
    with open(fname, "r") as StatFile:
        lines = StatFile.readlines()

    pim_command_stats = False
    for line in lines:
        if "PIM Command Stats" in line:
            pim_command_stats = True
            continue

        if pim_command_stats and "TOTAL -" in line:
            line_split = line.strip().split()

            runtime = line_split[4]
            energy = line_split[5]
            gops_w = line_split[6]
            read_percent = line_split[7]
            write_percent = line_split[8]
            l_percent = line_split[9]
            return float(runtime),float(energy)


    assert False, "Unreachable"


def get_full_pim_result_stats_from_pim_log_files(compute_fname, datamovement_fname):
    with open(compute_fname, "r") as StatFile:
        lines = StatFile.readlines()

    stats = {

    }

    pim_command_stats = False

    EVENTS = ["ACT", "PRE", "CAS", "Compute"]

    for line in lines:
        if "PIM Command Stats" in line:
            pim_command_stats = True
            continue

        if pim_command_stats and "TOTAL -" in line:
            line_split = line.strip().split()

            runtime = line_split[4]
            energy = line_split[5]
            gops_w = line_split[6]
            read_percent = line_split[7]
            write_percent = line_split[8]
            l_percent = line_split[9]

            stats['runtime'] = runtime
            stats['energy'] = energy
            stats['gops_w'] = gops_w
            stats['%R'] = read_percent
            stats["%W"] = write_percent
            stats["%L"] = l_percent

        for EVT in EVENTS:
            if f"TOTAL {EVT}:" in line:
                value = line.split(":")[-1].strip()
                stats[f'EVT_{EVT}_COUNT'] = value


    with open(datamovement_fname, "r") as StatFile:
        lines = StatFile.readlines()

    pim_command_stats = False
    for line in lines:
        if "Data Copy Stats" in line:
            pim_command_stats = True
            continue

        if "PIM Command Stats" in line:
            pim_command_stats = False
            continue

        if pim_command_stats and "TOTAL -" in line:
            line_split = line.split(":")[-1].strip().split()


            runtime = line_split[2]
            energy = line_split[6]

            stats['data_movement_runtime'] = runtime
            stats['data_movement_energy'] = energy


    return stats


def is_misaal_decl(line):
    tokens = line.split()
    if len(tokens) < 2:
        return False
    return tokens[1].startswith("misaal_node")


def compile_halide_benchmark(benchmark_name, cfg, perf_file_csv):
    tmp_dir = f"work_dir_{get_random_tempfile_name()}"
    os.mkdir(tmp_dir)
    try:
        VF = cfg['VECTORIZATION_FACTOR']
        RANK = cfg['RANK']
        BANKS_PER_RANK = cfg['BANKS_PER_RANK']
        SUBARRAYS_PER_BANK = cfg['SUBARRAYS_PER_BANK']
        NUM_ROWS = cfg['NUM_ROWS']
        NUM_COLS = cfg['NUM_COLS']
        VECTORIZATION_FACTOR = cfg['VECTORIZATION_FACTOR']
        DEVICE_TYPE = cfg['DEVICE_TYPE']

        GENERATOR_FILE_NAME = f"{tmp_dir}/{benchmark_name}_generator"
        RESULT_HEADER_FILE = f"{tmp_dir}/misaal_pim_lib.h"
        make_gen_cmd = f"g++ --std=c++17 -fno-rtti -O3 -DLOG2VLEN=7  -DVF={VF} -I {HALIDE_DISTRIB}/include -I {HALIDE_DISTRIB}/tools -g {benchmark_name}/src/{benchmark_name}_generator.cpp {HALIDE_DISTRIB}/tools/GenGen.cpp hannk/common_halide.cpp -o {GENERATOR_FILE_NAME} -L {HALIDE_DISTRIB}/lib -lHalide -lrt -ldl -lm -lz -lxml2"


        eq_sat_file = f"{benchmark_name}_{tmp_dir}_misaal*.py"

        compile_misaal_cmd = f"export LD_LIBRARY_PATH={HALIDE_DISTRIB}//lib;HL_EXPR_DEPTH=2  HYDRIDE_BENCHMARK={benchmark_name}_{tmp_dir}_misaal  PIM_HEADER_FILE={RESULT_HEADER_FILE} HL_ENABLE_MISAAL=1  MISAAL_EQ_SAT_ITERS=5 HL_ENABLE_HYDRIDE=1 HL_SYNTH_BW=16  HYDRIDE_INITIAL_HASH=\"empty_hash\"  MISAAL_DISABLE_FRONTEND_PATTERNS=1 COST_FILE_CSV_NAME={perf_file_csv} VF={VF} {GENERATOR_FILE_NAME} -t 0 -o {tmp_dir} -g {benchmark_name} -e cpp,h,stmt -f {benchmark_name} target=host-x86-64-no_bounds_query-no_asserts"

        compile_halide_runtime_cmd = f"HL_EXPR_DEPTH=2 HL_ENABLE_HYDRIDE=0 HL_DEBUG_CODEGEN=1  HL_SYNTH_BW=16  MISAAL_DISABLE_FRONTEND_PATTERNS=1 {GENERATOR_FILE_NAME} -r halide_runtime_x86 -o {tmp_dir} -e object,c_header target=host-x86-64-no_bounds_query-no_asserts"


        halide_cpp_fname = f"{tmp_dir}/{benchmark_name}.halide_generated.cpp"


        EXECUTABLE_NAME = f"{tmp_dir}/{benchmark_name}_run.out"
        gen_binary = f"g++ -DHALIDE_CPP_ALWAYS_USE_CPP_VECTORS -DFUSED -DVF={VF} -DNUM_RANKS={RANK} -DNUM_BPR={BANKS_PER_RANK} -DNUM_SPB={SUBARRAYS_PER_BANK} -DNUM_ROWS={NUM_ROWS} -DNUM_COLS={NUM_COLS} -Dbenchmark_{benchmark_name} --std=c++17 -O0 -march=native -mavx512vl -mavx512ifma -I {HALIDE_DISTRIB}/include -I {tmp_dir} -I ./ -lstdc++ -ldl -pthread test/run.cpp test/stubs.cpp {halide_cpp_fname} -L ./ -lpimeval {tmp_dir}/halide_runtime_x86.o -o {EXECUTABLE_NAME}"


        success = execute_cmd(make_gen_cmd)
        if not success:
            return 100000

        success = execute_cmd(compile_misaal_cmd)
        if not success:
            return 100000
        success = execute_cmd(compile_halide_runtime_cmd)
        if not success:
            return 100000

        with open(RESULT_HEADER_FILE, "r") as KernelFile:
            kernel_lines = KernelFile.readlines()
        with open(halide_cpp_fname, "r") as InputFile:
            input_lines = InputFile.readlines()

        previous_line_misaal_decl = False
        output_lines = []

        for idx, line in enumerate(input_lines):
            if idx == 0:
                output_lines.append(line)
                continue

            if is_misaal_decl(input_lines[idx - 1]) and not is_misaal_decl(input_lines[idx]):
                output_lines += kernel_lines

            output_lines.append(line)
        with open(halide_cpp_fname, "w") as OutputFile:
            OutputFile.write(" ".join(output_lines))

        success =  execute_cmd(gen_binary)
        if not success:
            return 100000


        LOGFileName = f"{tmp_dir}/output_log.txt"
        with open(LOGFileName, "w+") as LogFile:
            print(f"./{EXECUTABLE_NAME}")
            print(f"pipe to {LOGFileName}")
            sb.call(f"./{EXECUTABLE_NAME}", shell = True, stdout = LogFile, stderr = LogFile)


        exec_time, energy = get_result_stats_from_pim_log_file(LOGFileName)
        sb.call(f"rm -rf {tmp_dir}", shell = True)
        sb.call(f"rm -rf {eq_sat_file}", shell = True)
        return exec_time
    except Exception as e:
        print(e)
        #sb.call(f"rm -rf {tmp_dir}", shell = True)
        return 100000


def fix_header_type_decls(data):
    func_to_opnd_map = {}

    for line in data:
        if "misaal" not in line:
            continue

        if ";" not in line:
            continue

        if "=" in line:
            continue

        start_index = line.index("misaal")
        line = line[start_index:]


        bracket_start = line.index("(")
        bracket_end = line.index(")")

        func_name = line[:bracket_start]
        opnd_substr = line[bracket_start+1: bracket_end]

        opnd_types = opnd_substr.split(",")

        opnd_types = [op.strip() for op in opnd_types]

        func_to_opnd_map[func_name] = opnd_types


    for idx, line in enumerate(data):
        if "misaal" not in line:
            continue

        if "{" not in line:
            continue

        if "=" in line:
            continue

        orig_line = line

        start_index = line.index("misaal")

        return_type = line[:start_index].strip()
        line = line[start_index:]


        bracket_start = line.index("(")
        bracket_end = line.index(")")

        func_name = line[:bracket_start]
        opnd_substr = line[bracket_start+1: bracket_end]

        opnd_types = opnd_substr.split(",")
        opnd_types = [op.strip() for op in opnd_types]

        var_names = [opnd.split(" ")[-1] for opnd in opnd_types]
        expected_types = func_to_opnd_map[func_name]

        new_type_decl = [f"{expected_types[i]} {var_names[i]}" for i in range(len(var_names))]

        opnds = ", ".join(new_type_decl)

        new_line = f"{return_type} {func_name}({opnds})" + "{\n"
        data[idx] = new_line

    return data


def compile_halide_benchmark_from_cfg_file(benchmark_name, cfg, perf_file_csv, copy_code_path = None):
    tmp_dir = f"work_dir_{get_random_tempfile_name()}"
    os.mkdir(tmp_dir)
    try:
        VF = cfg['VECTORIZATION_FACTOR']
        FILE=cfg['FILE']

        base_name = os.path.basename(FILE)
        cfg_base_name = os.path.splitext(base_name)[0]

        # The simulator reads its config from $PIM_CONFIG (see benchmarks/test/run.cpp).
        FILE = os.path.abspath(FILE)
        run_env = dict(os.environ, PIM_CONFIG = FILE)

        GENERATOR_FILE_NAME = f"{tmp_dir}/{benchmark_name}_generator"
        RESULT_HEADER_FILE = f"{tmp_dir}/misaal_pim_lib.h"
        make_gen_cmd = f"g++ --std=c++17 -fno-rtti -O3 -DLOG2VLEN=7  -DVF={VF} -I {HALIDE_DISTRIB}/include -I {HALIDE_DISTRIB}/tools -g {BENCH_ROOT}/{benchmark_name}/src/{benchmark_name}_generator.cpp {HALIDE_DISTRIB}/tools/GenGen.cpp {BENCH_ROOT}/hannk/common_halide.cpp -o {GENERATOR_FILE_NAME} -L {HALIDE_DISTRIB}/lib -lHalide -lrt -ldl -lm -lz -lxml2"


        eq_sat_file = f"{benchmark_name}_{tmp_dir}_misaal*.py"

        compile_misaal_cmd = f"export LD_LIBRARY_PATH={HALIDE_DISTRIB}//lib;HL_EXPR_DEPTH=2  HYDRIDE_BENCHMARK={benchmark_name}_{tmp_dir}_misaal  PIM_HEADER_FILE={RESULT_HEADER_FILE} HL_ENABLE_MISAAL=1  MISAAL_EQ_SAT_ITERS=5 HL_ENABLE_HYDRIDE=1 HL_SYNTH_BW=16  HYDRIDE_INITIAL_HASH=\"empty_hash\"  MISAAL_DISABLE_FRONTEND_PATTERNS=1 COST_FILE_CSV_NAME={perf_file_csv} VF={VF} {GENERATOR_FILE_NAME} -t 0 -o {tmp_dir} -g {benchmark_name} -e cpp,h,stmt -f {benchmark_name} target=host-x86-64-no_bounds_query-no_asserts"

        compile_halide_runtime_cmd = f"HL_EXPR_DEPTH=2 HL_ENABLE_HYDRIDE=0 HL_DEBUG_CODEGEN=1  HL_SYNTH_BW=16  MISAAL_DISABLE_FRONTEND_PATTERNS=1 {GENERATOR_FILE_NAME} -r halide_runtime_x86 -o {tmp_dir} -e object,c_header target=host-x86-64-no_bounds_query-no_asserts"


        halide_cpp_fname = f"{tmp_dir}/{benchmark_name}.halide_generated.cpp"


        # Same flags as benchmarks/Makefile: the declarations-only lowering header
        # plus the prebuilt fused_lib.o, instead of compiling the full lowering
        # header into every binary.
        common_flags = f"-DHALIDE_CPP_ALWAYS_USE_CPP_VECTORS -Dbenchmark_{benchmark_name} --std=c++17 -O3 -march=native -mavx512vl -mavx512ifma -ffunction-sections -fdata-sections -I {HALIDE_DISTRIB}/include -I {LIBPIMSIM}/decls -I {LIBPIMSIM} -I {tmp_dir} -lstdc++ -ldl -pthread"
        common_inputs = f"{BENCH_ROOT}/test/run.cpp {halide_cpp_fname} {LIBPIMSIM}/fused_lib.o -L {LIBPIMSIM} -lpimeval -Wl,--gc-sections {tmp_dir}/halide_runtime_x86.o"

        EXECUTABLE_COMPUTE_NAME = f"{tmp_dir}/{benchmark_name}_compute_run.out"
        gen_compute_binary = f"g++ -DPROFILE_COMPUTE=1 -DFUSED {common_flags} {common_inputs} -o {EXECUTABLE_COMPUTE_NAME}"

        EXECUTABLE_DATA_NAME = f"{tmp_dir}/{benchmark_name}_data_run.out"
        gen_data_binary = f"g++ -DPROFILE_DATA_MOVEMENT=1 {common_flags} {common_inputs} -o {EXECUTABLE_DATA_NAME}"


        success = execute_cmd(make_gen_cmd)
        if not success:
            return 100000

        success = execute_cmd(compile_misaal_cmd)
        if not success:
            return 100000
        print("MISAAL COMPILATION SUCCESSFUL")
        success = execute_cmd(compile_halide_runtime_cmd)
        if not success:
            return 100000
        print("RUNTIME COMPILATION SUCCESSFUL")

        with open(RESULT_HEADER_FILE, "r") as KernelFile:
            kernel_lines = KernelFile.readlines()
        with open(halide_cpp_fname, "r") as InputFile:
            input_lines = InputFile.readlines()

        if copy_code_path is not None:
            # copy generated code for reference
            result_name = f"{copy_code_path}/{cfg_base_name}_{benchmark_name}_{VF}_header.h"
            execute_cmd(f"cp {RESULT_HEADER_FILE} {result_name}")

        previous_line_misaal_decl = False
        output_lines = []

        for idx, line in enumerate(input_lines):
            if idx == 0:
                output_lines.append(line)
                continue

            if is_misaal_decl(input_lines[idx - 1]) and not is_misaal_decl(input_lines[idx]):
                output_lines += kernel_lines

            output_lines.append(line)

        # Fix headers
        output_lines = fix_header_type_decls(output_lines)

        with open(halide_cpp_fname, "w") as OutputFile:
            OutputFile.write(" ".join(output_lines))

        success =  execute_cmd(gen_compute_binary)
        if not success:
            print("Failed to generate compute binary")
            return 100000

        success =  execute_cmd(gen_data_binary)
        if not success:
            print("Failed to generate data binary")
            return 100000


        for executable, suffix in ((EXECUTABLE_COMPUTE_NAME, "compute_log"),
                                   (EXECUTABLE_DATA_NAME, "data_log")):
            LOGFileName = f"{copy_code_path}/{cfg_base_name}_{benchmark_name}_{VF}_{suffix}"
            with open(LOGFileName, "w+") as LogFile:
                print(f"PIM_CONFIG={FILE} ./{executable}")
                print(f"pipe to {LOGFileName}")
                return_code = sb.call(f"./{executable}", shell = True, stdout = LogFile, stderr = LogFile, env = run_env)
            if return_code != 0:
                print(f"{executable} failed (exit {return_code}); see {LOGFileName}")
                return 100000


        exec_time, energy = get_result_stats_from_pim_log_file(LOGFileName)
        sb.call(f"rm -rf {tmp_dir}", shell = True)
        sb.call(f"rm -rf {eq_sat_file}", shell = True)
        return exec_time
    except Exception as e:
        print(e)
        #sb.call(f"rm -rf {tmp_dir}", shell = True)
        return 100000


def pim_eval_function(cfg, benchmark_name, copy_code_path = None):
    print(cfg)
    #{'RANK': 1, 'BANKS_PER_RANK': 2, 'SUBARRAYS_PER_BANK': 4, 'NUM_ROWS': 1024, 'NUM_COLS': 1024, 'VECTORIZATION_FACTOR': 64, 'DEVICE_TYPE': <PimDeviceEnum.PIM_DEVICE_BANK_LEVEL: 11>}
    RANK = cfg['RANK']
    BANKS_PER_RANK = cfg['BANKS_PER_RANK']
    SUBARRAYS_PER_BANK = cfg['SUBARRAYS_PER_BANK']
    NUM_ROWS = cfg['NUM_ROWS']
    NUM_COLS = cfg['NUM_COLS']
    VECTORIZATION_FACTOR = cfg['VECTORIZATION_FACTOR']
    DEVICE_TYPE = cfg['DEVICE_TYPE']
    FILE_NAME = cfg['FILE']
    csv_name = None
    GET_PERF = True
    if GET_PERF and FILE_NAME is not None:
        # Same cost-model step as the benchmark Makefile: reuse the CSV in
        # isa/perf_cost_model/perf_logs if present, generate it otherwise.
        cmd = f"python3 {ENSURE_COST_MODEL} --config {os.path.abspath(FILE_NAME)} --vf {VECTORIZATION_FACTOR} --out-dir {COST_CSV_ROOT} --libpimsim {LIBPIMSIM}"
        if not execute_cmd(cmd):
            print("Cost model generation failed, returning early")
            return {'time': 1000000}
        cfg_name = os.path.basename(FILE_NAME).split(".")[0]
        csv_name = os.path.join(COST_CSV_ROOT, f"pim_perf_results_config_{cfg_name}_vf{VECTORIZATION_FACTOR}.csv")
        print("CSV Produced for ", csv_name)
    elif GET_PERF:
        try:
            valid = evaluate_config(device_type = DEVICE_TYPE, ranks = RANK, bpr = BANKS_PER_RANK, spb = SUBARRAYS_PER_BANK, num_rows = NUM_ROWS, num_cols = NUM_COLS, vf = VECTORIZATION_FACTOR, config_file = FILE_NAME)
            print("evaluate config returned", valid)
            if not valid:
                print("Not valid pim config returning early")
                return {'time': 1000000}
            csv_name = get_csv_name(device_type = DEVICE_TYPE, ranks = RANK, bpr = BANKS_PER_RANK, spb = SUBARRAYS_PER_BANK, num_rows = NUM_ROWS, num_cols = NUM_COLS, vf = VECTORIZATION_FACTOR, config_file = FILE_NAME)
            print("CSV Produced for ", csv_name)
        except Exception as e:
            print("Error in get perf", e)
            return {'time': 1000000}

    if FILE_NAME is not None:
        pim_execution_time  = compile_halide_benchmark_from_cfg_file(benchmark_name, cfg, csv_name, copy_code_path = copy_code_path)
    else:
        pim_execution_time = compile_halide_benchmark(benchmark_name, cfg, csv_name)

    if  csv_name is not None and os.path.exists(csv_name):
        #os.remove(csv_name)
        pass

    print(f"Execution time for {cfg}: {pim_execution_time}")

    return {'time' : pim_execution_time}
