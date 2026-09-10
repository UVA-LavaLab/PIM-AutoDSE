#include "Halide.h"

using namespace Halide;

class FilterByKey : public Generator<FilterByKey> {
public:
    Input<Buffer<int16_t>> Input{ "Input", 1 };
    Output<Buffer<int16_t>> Output{ "Output", 1 };

    void generate() {
        int16_t key = 56;
        Output(x) =  select(key > Input(x), cast<int16_t> (1), cast<int16_t>(0));

        // Schedules for BitSIMD 
        Output
            .compute_root()
            .vectorize(x, 524288)
            ;

    }
private:
    Var x{ "x" }, y{ "y" };
};
HALIDE_REGISTER_GENERATOR(FilterByKey, filter_by_key)
