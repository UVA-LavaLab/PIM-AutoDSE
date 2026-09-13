from GenPimFusedCost import evaluate_config
import os
import glob
import concurrent.futures
import sys

pim_eval_path = os.environ.get("PIM_EVAL_ROOT", os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "libpimeval"))


# Sweep configs live under dse/cfgs/, independent of the working directory.
CFG_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cfgs")

config_files = []
#config_files += glob.glob(os.path.join(CFG_ROOT, "cfg_files_v2", "*.cfg"))
#config_files += glob.glob(os.path.join(CFG_ROOT, "cfg_reg_sweep_files", "*.cfg"))
config_files += glob.glob(os.path.join(CFG_ROOT, "cfg_files_scalar_sweep", "*.cfg"))
print("Config Files", config_files)


#VFS = [pow(2,i) for i in range(3, 25)]

VFS = [4096, 8192, 16384,65536 //2,   65536]
#VFS = [32768]


def worker(datum):
    cfg , VF = datum
    evaluate_config(config_file = cfg, vf = VF)

POOL_SIZE = 12

if True:
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=POOL_SIZE)
    for VF in  VFS:
        for cfg in config_files:
            if "AiM" in cfg:
                continue

            pool.submit(worker, (cfg, VF))
    pool.shutdown(wait=True)

else:
    for VF in  VFS:
        for cfg in config_files:
            if "AiM" in cfg:
                continue
            worker((cfg, VF))


