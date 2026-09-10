
import subprocess as sb
import sys

benchmark = sys.argv[1]
source_file = f"{benchmark}/bin/{benchmark}.halide_generated.cpp"
copy_file = f"{benchmark}/bin/{benchmark}.copy.halide_generated.cpp"
new_file = f"{benchmark}/bin/{benchmark}.updated.halide_generated.cpp"

sb.call(f"cp {source_file} {copy_file}", shell = True)

with open(source_file, "r") as InputFile:
    data = InputFile.readlines()



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


with open(source_file, "w+") as WriteFile:
    lines = "".join(data)
    WriteFile.write(lines)

