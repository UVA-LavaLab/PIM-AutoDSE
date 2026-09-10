import tempfile
import glob

import os
import sys

from PIM_TUNER_UTILS import pim_eval_function
from PIM_API_UTILS import PimDeviceEnum

import concurrent.futures

def evaluate_benchmark(benchmark_name, VF, cfg_file_path, log_path):

    cfg = {
        'RANK': None,
        'BANKS_PER_RANK': None,
        'SUBARRAYS_PER_BANK': None,
        'NUM_ROWS': None,
        'NUM_COLS': None,
        'VECTORIZATION_FACTOR': VF,
        'DEVICE_TYPE': PimDeviceEnum.PIM_FUNCTIONAL,
        'FILE': cfg_file_path
    }

    if not os.path.exists(cfg_file_path):
        print(f"{cfg_file_path} does not exist, early exiting")
        return

    pim_eval_function(cfg, benchmark_name, copy_code_path = log_path)

def worker(cfg):
    benchmark, VF, CFG_FILE, log_path = cfg
    evaluate_benchmark(benchmark, VF, CFG_FILE, log_path)

benchmark_to_VF = {
    "histogram": 8192,
    "gemv_v1": 4096,
    "gemm_small": 16384 ,
    "radix_sort": 262144 // 4,
    "softmax": 1024,
}



if __name__ == "__main__":

    POOL_SIZE = 6
    #benchmarks = ["relu", "histogram", "axpy", "filter_by_key", "gemv_v1", "gemv_v2", "gemv_v3", "gemm_small","radix_sort"]
    benchmarks = ["softmax", "convolution"]
    VFS = [1024]
    CFG_FILES = glob.glob("./autodse_eval_cfgs/*.cfg")

    TESTS = [
        #"bank-simd_mem-ddr_ranks-20_banks-32_subarr-32_rows-1024_cols-2048",
        "vregc_2",
    ]

    #CFG_FILES = [f for f in CFG_FILES if any([t in f for t in TESTS])]


    log_path = "autodse_main_out"
    if not os.path.exists(log_path):
        os.mkdir(log_path)

    PARALLEL = True
    if PARALLEL:
        pool = concurrent.futures.ThreadPoolExecutor(max_workers=POOL_SIZE)
        for benchmark in benchmarks:
            for VF in VFS:
                for CFG_FILE in CFG_FILES:
                    pool.submit(worker, (benchmark, benchmark_to_VF[benchmark] if benchmark in benchmark_to_VF else 32768, CFG_FILE, log_path))

        pool.shutdown(wait=True)
    else:
        for benchmark in benchmarks:
            for VF in VFS:
                for CFG_FILE in CFG_FILES:
                    worker((benchmark, benchmark_to_VF[benchmark] if benchmark in benchmark_to_VF else 32768 , CFG_FILE, log_path))




