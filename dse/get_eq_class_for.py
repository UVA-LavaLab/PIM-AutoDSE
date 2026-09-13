

from sema.bitserial_fused_sema_v2 import bitserial_fused_sema_v2 as bitserial_fused_sema
from utils.DSLInstructionUtils import parse_dict_with_bounded

import sys


inst_dict = parse_dict_with_bounded(bitserial_fused_sema)
ctx_name = "test_enum_2_"+sys.argv[1]


for dsl_inst in inst_dict:
    for ctx in dsl_inst.contexts:
        if ctx.name == ctx_name:
            print(dsl_inst.name)
