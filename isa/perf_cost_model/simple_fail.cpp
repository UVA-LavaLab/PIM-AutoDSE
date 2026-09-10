// Automatically generated file
#include "libpimeval.h"
#include <stdio.h>

#define VF 16384


/*
   (comb_1_fused_pim_op_1
   (LoopOp
   (TruncateOp 			(a 16 128)
   )
   )
   )
   */
void comb_1_fused_pim_op_1(void* reg_0, int64_t reg_0_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

    PimProg prog;
    // Emitting Allocations
    PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, reg_0_num_elems, PIM_INT16);
    PimObjId fuse_expr_0 = pimAllocAssociated(fuse_root, PIM_INT8);
    // Emitting Copy Host to Device
    prog.add(pimCopyHostToDevice,(void*)ret_vec, fuse_expr_0, 0UL, 0UL);
    // Creating PIM Fused Program
    prog.add(pimConvertType,fuse_root , fuse_expr_0);
    // Emitting Copy Device to Host
    prog.add(pimCopyDeviceToHost,fuse_root,(void*)reg_0, 0UL, 0UL);
    pimFuse(prog);
    // Emitting Deallocations
    pimFree(fuse_root);
    pimFree(fuse_expr_0);
}

void benchmark_comb_1_fused_pim_op_1(){
    printf("Benchmarking comb_1_fused_pim_op_1\n");
    pimResetStats();
    if(VF == 0){
        int16_t arg_0[8];
        int8_t ret_val[8];
        comb_1_fused_pim_op_1(arg_0, 8, ret_val, 8);

    } else {
        int16_t arg_0[VF];
        int8_t ret_val[VF];
        comb_1_fused_pim_op_1(arg_0, VF, ret_val, VF);

    };
    pimShowStats();
}



/*
(comb_8_fused_pim_op_273
	(LoopOp
		(LTOp
			(BroadcastOp 				(a 16 16)
 64)
 			(ZeroExtendOp 				(b 8 512)
)
)
)
 )
*/
void comb_8_fused_pim_op_273(int64_t reg_0, int64_t reg_0_num_elems, void* reg_1, int64_t reg_1_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

	PimProg prog;
	// Emitting Allocations
	PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, reg_0_num_elems, PIM_INT16);
	PimObjId fuse_expr_0 = pimAllocAssociated(fuse_root, PIM_BOOL);
	// Scalar operand fuse_expr_2 does not need pim allocation
	auto fuse_expr_2 = reg_0;
	PimObjId fuse_expr_3 = pimAllocAssociated(fuse_root, PIM_INT16);
	PimObjId fuse_expr_4 = pimAllocAssociated(fuse_root, PIM_INT8);
	// Emitting Copy Host to Device
	// No need to copy scalar value reg_0 to PIM memory
	prog.add(pimCopyHostToDevice,(void*)reg_1, fuse_expr_4, 0UL, 0UL);
	// Creating PIM Fused Program
	prog.add(pimConvertType,fuse_expr_4 , fuse_expr_3);
	prog.add(pimBroadcastInt, fuse_root ,fuse_expr_2);
	prog.add(pimLT,fuse_root, fuse_expr_3 , fuse_expr_0);
	// Emitting Copy Device to Host
	prog.add(pimCopyDeviceToHost,fuse_expr_0,(void*)ret_vec, 0UL, 0UL);
	pimFuse(prog);
	// Emitting Deallocations
	pimFree(fuse_expr_0);
	pimFree(fuse_root);
	// Scalar operand fuse_expr_2 does not need pim deallocation
	pimFree(fuse_expr_3);
	pimFree(fuse_expr_4);
}
void benchmark_comb_8_fused_pim_op_273(){
printf("Benchmarking comb_8_fused_pim_op_273\n");
pimResetStats();
if(VF == 0){
int16_t arg_0;
int8_t arg_1[64];
int8_t ret_val[64];
comb_8_fused_pim_op_273(arg_0, 1, arg_1, 64, ret_val, 64);

} else {
 int16_t arg_0;
int8_t arg_1[VF];
int8_t ret_val[VF];
comb_8_fused_pim_op_273(arg_0, VF, arg_1, VF, ret_val, VF);

};
pimShowStats();
}


int main(){
    unsigned numRanks = 1;
    unsigned numBankPerRank = 1;
    unsigned numSubarrayPerBank = 8;
    unsigned numRows = 1024;
    unsigned numCols = 8192;
    PimStatus status = pimCreateDevice(PIM_DEVICE_BANK_LEVEL, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
    benchmark_comb_1_fused_pim_op_1();
    benchmark_comb_8_fused_pim_op_273();
    return 0;
}
