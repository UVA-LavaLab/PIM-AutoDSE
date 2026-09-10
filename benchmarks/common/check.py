#(test_enum_1_comb_12_fused_pim_op_191 (LIT "1" 1) (LIT "0" 1) (reg (bv 0 8)) (reg (bv 0 8)) (reg 1) 524288 524288 0 524288 1 0 16 16 0)

from sema.pim_extend_dsl import pim_extended_dsl as bitserial_fused_sema
from common.DSLParser import parse_dict
from utils.DSLInstructionUtils import get_dsl_inst_from_dsl_list

pim_dsl_list = parse_dict(bitserial_fused_sema )

dsl_inst = get_dsl_inst_from_dsl_list( "test_enum_1_comb_12_fused_pim_op_191", pim_dsl_list)


for ctx in dsl_inst.contexts:
    any_of_req_value = any([arg for arg in ctx.context_args if hasattr(arg, 'value') and arg.value == 524288])

    if any_of_req_value:
        print(ctx.emit_context_expr_string())


