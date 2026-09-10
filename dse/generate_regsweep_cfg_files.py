import glob
import itertools
import os

config_files = glob.glob(f"cfg_files/*.cfg")

#RESULT_DIR="./cfg_reg_sweep_files/"
#RESULT_DIR="./cfg_files_v2/"
RESULT_DIR="./cfg_files_scalar_sweep/"

if not os.path.exists(RESULT_DIR):
    os.mkdir(RESULT_DIR)


if False:
    vreg_bws=  [64, 128, 256,  512,  2048, 4096, 8192]
else:

    vreg_bws=  [8192]


vreg_counts = [ 3]#range(2, 4+1)
sregs_bws = [32]
sregs_counts = [1]



for f in config_files:

    print(f)
    base_name = os.path.basename(f).split(".cfg")[0]
    with open(f, "r") as DataFile:
        content = DataFile.read()

    combination_iter = itertools.product(vreg_bws, vreg_counts, sregs_bws, sregs_counts)
    for (vreg_b, vreg_c, sreg_b, sreg_c) in combination_iter:
        #print((vreg_b, vreg_c, sreg_b, sreg_c))
        result_name = f"{base_name}_vregbw_{vreg_b}_vregc_{vreg_c}_sregbw_{sreg_b}_sregc_{sreg_c}.cfg"

        print(f"{RESULT_DIR}/{result_name}")
        with open(f"{RESULT_DIR}/{result_name}", "w+") as WFile:
            WFile.write(content+"\n")
            WFile.write(f"vector_register_count = {vreg_c}\n")
            WFile.write(f"vector_register_bitwidth = {vreg_b}\n")
            WFile.write(f"scalar_register_count = {sreg_c}\n")
            WFile.write(f"scalar_register_bitwidth = {sreg_b}\n")




