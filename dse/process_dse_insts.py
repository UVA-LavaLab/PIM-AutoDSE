import glob
import json
import itertools
import sys

log_dir = 'dse_perf_logs_vreg'
log_dir = 'dse_logs_perf_opt'
log_dir = "autodse_main_out"
COUNTER = 0

benchmark = "softmax" #["gemm_small", "gemv_v1", "histogram", "radix_sort"][2]

header_files = glob.glob(f"{log_dir}/*{benchmark}*header.h")
print("Num headers: ", len(header_files))

def create_histogram(data):
    histogram = {}
    for file_, ops in data.items():
        unique_ops = list(set(ops))
        assert len(unique_ops) == len(ops)
        for op in set(ops):
            if op not in histogram:
                histogram[op] = 0
            histogram[op] += 1
    return histogram

distribution = {}
op_to_file_map = {}
func_name_to_ops = {}
func_to_file_name = {}

for file_ in header_files:
    print(file_)

    with open(file_, "r") as File:
        content = File.readlines()

    ops = set()


    func_name = None
    for line in content:
        line = line.strip()
        if "misaal" in line:
            func_name = line.split("(")[0].split("_t ")[1]
            print(line)
            print("Func name:", func_name)

            if func_name not in func_name_to_ops:
                func_name_to_ops[func_name] = []
            if func_name not in func_to_file_name:
                func_to_file_name[func_name] = file_


        if line.startswith("test_") or line.startswith("comb"):
            op_name = line.split("(")[0]

            if op_name not in op_to_file_map:
                op_to_file_map[op_name] = []

            if file_ not in op_to_file_map[op_name]:
                op_to_file_map[op_name].append(file_)

            #if op_name not in func_name_to_ops[func_name]:
            func_name_to_ops[func_name].append(op_name)


            ops.add(op_name)
    distribution[file_] = list(ops)



print("Num Dist keys", len(distribution))
print(json.dumps(distribution, indent = 4))

histogram = create_histogram(distribution)
print(benchmark)
print(json.dumps(histogram, indent = 4))



print(json.dumps(op_to_file_map, indent = 4))


def process_test_op_info(op_name):
    UNUSED = False
    for file_ in header_files:
        if file_ not in op_to_file_map[op_name]:
            UNUSED = True
            print(f"{file_} doesn't use {op_name}")

    print(f"Unused: {UNUSED}")

#process_test_op_info(TEST_OP)

print(json.dumps(func_name_to_ops, indent = 4))

def deduplicate_list_of_list_of_strings(ls):
    unique_list = []

    for x in ls:
        if x in unique_list:
            continue
        unique_list.append(x)
    return unique_list

def process_op_dist_across_functions():

    unique_counts = {}
    for func_name in func_name_to_ops:
        counter = func_name.split("_")[-1]

        # Sort all the values
        func_name_to_ops[func_name] = sorted(func_name_to_ops[func_name])

        if counter not in unique_counts:
            unique_counts[counter] = []

        unique_counts[counter].append(func_name)


    total_count = len(header_files)
    for counter in unique_counts:

        if int(counter) != COUNTER:
            continue
        print(f"Counter:\t{counter}")


        values = [func_name_to_ops[func_name] for func_name in unique_counts[counter]]
        print("values", values)
        #unique_combs = list(k for k,_ in itertools.groupby(values))
        unique_combs = deduplicate_list_of_list_of_strings(values)
        print("Unique combinations:", len(unique_combs))
        if len(unique_combs) > 1:
            print("Multiple unique")
            #sys.exit(0)

        if len(unique_combs) > 1:

            for idx, comb in enumerate(unique_combs):
                print(f"Combination {idx + 1}:", comb)
            print("="* 50)
            for idx, comb in enumerate(unique_combs):
                print(f"Combination {idx + 1}:", comb)

                count = 0
                for func_name, vals in func_name_to_ops.items():

                    if vals == comb and func_name.endswith(f"_{counter}"):
                        count += 1
                        #print(f"- {func_name} in {func_to_file_name[func_name]}")
                        file_name = func_to_file_name[func_name].split("/")[-1]
                        print(f"{file_name}")
                print(f"Count for this combination: {count}/{total_count}")



process_op_dist_across_functions()




