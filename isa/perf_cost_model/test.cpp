// Automatically generated file
#include "libpimeval.h"
#include "get_perf_stats.h"

int main(){
    unsigned numRanks = 1;
    unsigned numBankPerRank = 1;
    unsigned numSubarrayPerBank = 8;
    unsigned numRows = 1024;
    unsigned numCols = 8192;
    PimStatus status = pimCreateDevice(PIM_DEVICE_BANK_LEVEL, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
    get_perf();
    return 0;
}
