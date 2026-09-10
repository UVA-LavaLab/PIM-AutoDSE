// Automatically generated file
#include "libpimeval.h"
#include <cstdio>
#ifndef VF 
#define VF 8
#endif 

/*
   (comb_0_fused_pim_op_0
   (LoopOp
   (LTOp
   (a 8 512)
   (b 8 512)
   )
   )
   )
   */
void comb_0_fused_pim_op_0(void* reg_0, int64_t reg_0_num_elems, void* reg_1, int64_t reg_1_num_elems, void* ret_vec, int64_t ret_vec_num_elems){

    PimProg prog;
    // Emitting Allocations
    PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, reg_0_num_elems, PIM_INT8);
    PimObjId fuse_expr_0 = pimAllocAssociated(fuse_root, PIM_BOOL);
    PimObjId fuse_expr_2 = pimAllocAssociated(fuse_root, PIM_INT8);
    // Emitting Copy Host to Device
    prog.add(pimCopyHostToDevice,(void*)reg_0, fuse_root, 0UL, 0UL);
    prog.add(pimCopyHostToDevice,(void*)reg_1, fuse_expr_2, 0UL, 0UL);
    // Creating PIM Fused Program
    prog.add(pimLT,fuse_root, fuse_expr_2 , fuse_expr_0);
    // Emitting Copy Device to Host
    prog.add(pimCopyDeviceToHost,fuse_expr_0,(void*)ret_vec, 0UL, 0UL);
    pimFuse(prog);
    // Emitting Deallocations
    pimFree(fuse_expr_0);
    pimFree(fuse_root);
    pimFree(fuse_expr_2);
}
void benchmark_comb_0_fused_pim_op_0(){
    printf("Benchmarking comb_0_fused_pim_op_0\n");
    pimResetStats();
    if(VF == 0){
        int8_t arg_0[64];
        int8_t arg_1[64];
        int8_t ret_val[64];
        comb_0_fused_pim_op_0(arg_0, 64, arg_1, 64, ret_val, 64);

    } else {
        int8_t* arg_0 = new int8_t[VF];
        int8_t* arg_1 = new int8_t[VF];
        int8_t* ret_val = new int8_t[VF];
        comb_0_fused_pim_op_0(arg_0, VF, arg_1, VF, ret_val, VF);
        delete[] arg_0;
        delete[] arg_1;
        delete[] ret_val;

    };
    pimShowStats();
}

int main(){
    {
        const char *cfg = getenv("PIM_CONFIG");
        if (cfg == nullptr) cfg = "PIMeval_Bank_Rank1_GDDR.cfg";
        pimCreateDeviceFromConfig(PIM_FUNCTIONAL, cfg);
    }
    benchmark_comb_0_fused_pim_op_0();
    return 0;
}

