// Automatically generated file
#include "libpimeval.h"

#include <stdio.h>


/*
(comb_31_fused_pim_op_1360
	(LoopOp
		(MaxOp
			(MinOp
				(a 32 512)
 				(b 32 512)
)
 			(BroadcastOp 				(c 32 32)
 16)
)
)
 )
*/
void comb_31_fused_pim_op_1360(void* reg_0, int64_t reg_0_num_elems, void* reg_1, int64_t reg_1_num_elems, int64_t reg_2, int64_t reg_2_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

	PimFusionBlock prog;
	// Emitting Allocations
	PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, ret_vec_num_elems, PIM_INT32);
	PimObjId fuse_expr_1 = pimAllocAssociated(fuse_root, PIM_INT32);
	PimObjId fuse_expr_2 = pimAllocAssociated(fuse_root, PIM_INT32);
	PimObjId fuse_expr_3 = pimAllocAssociated(fuse_root, PIM_INT32);
	PimObjId fuse_expr_4 = pimAllocAssociated(fuse_root, PIM_INT32);
	// Scalar operand fuse_expr_5 does not need pim allocation
	auto fuse_expr_5 = reg_2;
	// Emitting Copy Host to Device
	prog.add(pimCopyHostToDevice,(void*)reg_0, fuse_expr_2, 0UL, 0UL);
	prog.add(pimCopyHostToDevice,(void*)reg_1, fuse_expr_3, 0UL, 0UL);
	// No need to copy scalar value reg_2 to PIM memory
	// Creating PIM Fused Program
	prog.add(pimBroadcastInt, fuse_expr_4 ,fuse_expr_5);
	prog.add(pimMin,fuse_expr_2, fuse_expr_3 , fuse_expr_1);
	prog.add(pimMax,fuse_expr_1, fuse_expr_4 , fuse_root);
	// Emitting Copy Device to Host
	prog.add(pimCopyDeviceToHost,fuse_root,(void*)ret_vec, 0UL, 0UL);
	pimFuse(prog);
	// Emitting Deallocations
	pimFree(fuse_root);
	pimFree(fuse_expr_1);
	pimFree(fuse_expr_2);
	pimFree(fuse_expr_3);
	pimFree(fuse_expr_4);
	// Scalar operand fuse_expr_5 does not need pim deallocation
}
void benchmark_comb_31_fused_pim_op_1360(){
printf("Benchmarking comb_31_fused_pim_op_1360\n");
if(VF == 0){
int32_t arg_0[16];
int32_t arg_1[16];
int32_t arg_2;
int32_t ret_val[16];
comb_31_fused_pim_op_1360(arg_0, 16, arg_1, 16, arg_2, 1, ret_val, 16);

} else {
 int32_t* arg_0 = new int32_t[VF];
int32_t* arg_1 = new int32_t[VF];
int32_t arg_2;
int32_t* ret_val = new int32_t[VF];
comb_31_fused_pim_op_1360(arg_0, VF, arg_1, VF, arg_2, VF, ret_val, VF);
delete[] arg_0;
delete[] arg_1;
delete[] ret_val;

};
}


/*
(comb_15_fused_pim_op_0
	(LoopOp
		(BroadcastOp 			(a 32 32)
 16)
)
 )
*/
void comb_15_fused_pim_op_0(int64_t reg_0, int64_t reg_0_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

	PimFusionBlock prog;
	// Emitting Allocations
	PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, ret_vec_num_elems, PIM_INT32);
	// Scalar operand fuse_expr_1 does not need pim allocation
	auto fuse_expr_1 = reg_0;
	// Emitting Copy Host to Device
	// No need to copy scalar value reg_0 to PIM memory
	// Creating PIM Fused Program
	prog.add(pimBroadcastInt, fuse_root ,fuse_expr_1);
	// Emitting Copy Device to Host
	prog.add(pimCopyDeviceToHost,fuse_root,(void*)ret_vec, 0UL, 0UL);
	pimFuse(prog);
	// Emitting Deallocations
	pimFree(fuse_root);
	// Scalar operand fuse_expr_1 does not need pim deallocation
}
void benchmark_comb_15_fused_pim_op_0(){
printf("Benchmarking comb_15_fused_pim_op_0\n");
if(VF == 0){
int32_t arg_0;
int32_t ret_val[16];
comb_15_fused_pim_op_0(arg_0, 1, ret_val, 16);

} else {
 int32_t arg_0;
int32_t* ret_val = new int32_t[VF];
comb_15_fused_pim_op_0(arg_0, VF, ret_val, VF);
delete[] ret_val;

};
}


/*
(comb_23_fused_pim_op_1102
	(LoopOp
		(MaxOp
			(BroadcastOp 				(a 32 32)
 4)
 			(b 32 128)
)
)
 )
*/
void comb_23_fused_pim_op_1102(int64_t reg_0, int64_t reg_0_num_elems, void* reg_1, int64_t reg_1_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

	PimFusionBlock prog;
	// Emitting Allocations
	PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, ret_vec_num_elems, PIM_INT32);
	PimObjId fuse_expr_1 = pimAllocAssociated(fuse_root, PIM_INT32);
	// Scalar operand fuse_expr_2 does not need pim allocation
	auto fuse_expr_2 = reg_0;
	PimObjId fuse_expr_3 = pimAllocAssociated(fuse_root, PIM_INT32);
	// Emitting Copy Host to Device
	// No need to copy scalar value reg_0 to PIM memory
	prog.add(pimCopyHostToDevice,(void*)reg_1, fuse_expr_3, 0UL, 0UL);
	// Creating PIM Fused Program
	prog.add(pimBroadcastInt, fuse_expr_1 ,fuse_expr_2);
	prog.add(pimMax,fuse_expr_1, fuse_expr_3 , fuse_root);
	// Emitting Copy Device to Host
	prog.add(pimCopyDeviceToHost,fuse_root,(void*)ret_vec, 0UL, 0UL);
	pimFuse(prog);
	// Emitting Deallocations
	pimFree(fuse_root);
	pimFree(fuse_expr_1);
	// Scalar operand fuse_expr_2 does not need pim deallocation
	pimFree(fuse_expr_3);
}
void benchmark_comb_23_fused_pim_op_1102(){
printf("Benchmarking comb_23_fused_pim_op_1102\n");
if(VF == 0){
int32_t arg_0;
int32_t arg_1[4];
int32_t ret_val[4];
comb_23_fused_pim_op_1102(arg_0, 1, arg_1, 4, ret_val, 4);

} else {
 int32_t arg_0;
int32_t* arg_1 = new int32_t[VF];
int32_t* ret_val = new int32_t[VF];
comb_23_fused_pim_op_1102(arg_0, VF, arg_1, VF, ret_val, VF);
delete[] arg_1;
delete[] ret_val;

};
}

/*
(comb_27_fused_pim_op_1444
	(LoopOp
		(MinOp
			(BroadcastOp 				(a 32 32)
 8)
 			(b 32 256)
)
)
 )
*/
void comb_27_fused_pim_op_1444(int64_t reg_0, int64_t reg_0_num_elems, void* reg_1, int64_t reg_1_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

	PimFusionBlock prog;
	// Emitting Allocations
	PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, ret_vec_num_elems, PIM_INT32);
	PimObjId fuse_expr_1 = pimAllocAssociated(fuse_root, PIM_INT32);
	// Scalar operand fuse_expr_2 does not need pim allocation
	auto fuse_expr_2 = reg_0;
	PimObjId fuse_expr_3 = pimAllocAssociated(fuse_root, PIM_INT32);
	// Emitting Copy Host to Device
	// No need to copy scalar value reg_0 to PIM memory
	prog.add(pimCopyHostToDevice,(void*)reg_1, fuse_expr_3, 0UL, 0UL);
	// Creating PIM Fused Program
	prog.add(pimBroadcastInt, fuse_expr_1 ,fuse_expr_2);
	prog.add(pimMin,fuse_expr_1, fuse_expr_3 , fuse_root);
	// Emitting Copy Device to Host
	prog.add(pimCopyDeviceToHost,fuse_root,(void*)ret_vec, 0UL, 0UL);
	pimFuse(prog);
	// Emitting Deallocations
	pimFree(fuse_root);
	pimFree(fuse_expr_1);
	// Scalar operand fuse_expr_2 does not need pim deallocation
	pimFree(fuse_expr_3);
}
void benchmark_comb_27_fused_pim_op_1444(){
printf("Benchmarking comb_27_fused_pim_op_1444\n");
if(VF == 0){
int32_t arg_0;
int32_t arg_1[8];
int32_t ret_val[8];
comb_27_fused_pim_op_1444(arg_0, 1, arg_1, 8, ret_val, 8);

} else {
 int32_t arg_0;
int32_t* arg_1 = new int32_t[VF];
int32_t* ret_val = new int32_t[VF];
comb_27_fused_pim_op_1444(arg_0, VF, arg_1, VF, ret_val, VF);
delete[] arg_1;
delete[] ret_val;

};
}


void test_first_comb(){
    pimResetStats();
    benchmark_comb_31_fused_pim_op_1360();
    benchmark_comb_15_fused_pim_op_0();
    pimShowStats();
}

void test_second_comb(){
    pimResetStats();
    benchmark_comb_23_fused_pim_op_1102();
    benchmark_comb_27_fused_pim_op_1444();
    benchmark_comb_15_fused_pim_op_0();
    pimShowStats();

}

