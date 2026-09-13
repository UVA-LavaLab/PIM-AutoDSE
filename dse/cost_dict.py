import json
entries = []

with open("fused_lower.h","r") as HeaderFile:
    data = ""
    started = False
    for line in HeaderFile:
        if "/*" in line:
            started = True
            data = ""
            continue

        if "*/" in line:
            started = False
            entries.append(data)
            continue

        if started:
            data += line

dict_ = {}
for entry in entries:
    name = entry.split("\n")[0][1:]
    dict_[name] = entry




with open("fused_op_dict.json", "w+") as JsonFile:
    JsonFile.write(json.dumps(dict_, indent = 4))
