#include "Halide.h"

using namespace Halide;

class AXPY : public Generator<AXPY> {
public:
    Input<Buffer<int32_t>> A{ "A", 2 };
    Input<Buffer<int32_t>> B{ "B", 2 };
    Output<Buffer<int32_t>> output{ "output", 2 };

    void generate() {
        output(x, y) = (5 * A(x,y)) + B(x,y);
        output
            .compute_root()
            .reorder({y,x})
            .vectorize(x, 65536)
            ;

    }
private:
    Var x{ "x" }, y{ "y" };
};
HALIDE_REGISTER_GENERATOR(AXPY, axpy)
