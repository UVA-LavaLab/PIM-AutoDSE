#include "Halide.h"

using namespace Halide;

#define BATCH 2
#define M 1024
#define N 1024
#define K 1024

class Matmul : public Generator<Matmul> {
public:
    GeneratorParam<int> matrix_size{"size", 1024};
    Input<Buffer<int32_t>> A{ "A", 3 };
    Input<Buffer<int32_t>> B{ "B", 3 };
    Output<Buffer<int32_t>> output{ "output", 3 };

    void generate() {
        RDom k(0, K);

        output(b, x, y) = 0;
        output(b, x, y) += A(b,x,k) * B(b,k,y);
        // Schedules for BitSIMD 
        output
            .update(0)
            .specialize(A.dim(0).extent() == BATCH)
            .specialize(A.dim(1).extent() == M)
            .specialize(A.dim(2).extent() == K)
            .specialize(B.dim(0).extent() == BATCH)
            .specialize(B.dim(1).extent() == K)
            .specialize(B.dim(2).extent() == N)
            .specialize(output.dim(0).extent() == BATCH)
            .specialize(output.dim(1).extent() == M)
            .specialize(output.dim(2).extent() == N)
            .fuse(b, x, x)
            .fuse(x, y, x)
            .vectorize(x,M * N * BATCH)
            .unroll(k, 16)
            ;

    }

    void schedule() {}

private:
    Var x{ "x" }, y{ "y" }, b{"b"};
};

HALIDE_REGISTER_GENERATOR(Matmul, batched_gemm_v1)
