// Test: C++ version of matrix vector multiplication
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <iostream>
#include <vector>
#include <getopt.h>
#include <stdint.h>
#include <iomanip>
#if defined(_OPENMP)
#include <omp.h>
#endif

#include "util.h"
#include "libpimeval.h"

// Params ---------------------------------------------------------------------
typedef struct Params
{
  uint64_t row, column;
  char *configFile;
  char *inputFile;
  bool shouldVerify;
} Params;

void usage()
{
  fprintf(stderr,
          "\nUsage:  ./gemv.out [options]"
          "\n"
          "\n    -r    matrix row (default=2048 elements)"
          "\n    -d    matrix column (default=64 elements)"
          "\n    -c    dramsim config file"
          "\n    -i    input file containing two vectors (default=generates vector with random numbers)"
          "\n    -v    t = verifies PIM output with host output. (default=false)"
          "\n");
}

struct Params getInputParams(int argc, char **argv)
{
  struct Params p;
  p.row = 2048;
  p.column = 64;
  p.configFile = nullptr;
  p.inputFile = nullptr;
  p.shouldVerify = false;

  int opt;
  while ((opt = getopt(argc, argv, "h:r:d:c:i:v:")) >= 0)
  {
    switch (opt)
    {
    case 'h':
      usage();
      exit(0);
      break;
    case 'r':
      p.row = strtoull(optarg, NULL, 0);
      break;
    case 'd':
      p.column = strtoull(optarg, NULL, 0);
      break;
    case 'c':
      p.configFile = optarg;
      break;
    case 'i':
      p.inputFile = optarg;
      break;
    case 'v':
      p.shouldVerify = (*optarg == 't') ? true : false;
      break;
    default:
      fprintf(stderr, "\nUnrecognized option!\n");
      usage();
      exit(0);
    }
  }
  return p;
}

void gemv(uint64_t row, uint64_t col, std::vector<int> &srcVector, std::vector<std::vector<int>> &srcMatrix, std::vector<int> &dst)
{
  PimFusionBlock prog;
  // Emitting Allocations
  PimObjId fuse_root = pimAlloc(PIM_ALLOC_AUTO, col, PIM_INT8);
  PimObjId fuse_expr_0 = pimAllocAssociated(fuse_root, PIM_BOOL);
  PimObjId fuse_expr_2 = pimAllocAssociated(fuse_root, PIM_INT8);
  // Emitting Copy Host to Device
  prog.add(pimCopyHostToDevice,(void*)srcVector.data(), fuse_root, 0UL, 0UL);
  prog.add(pimCopyHostToDevice,(void*)srcVector.data(), fuse_expr_2, 0UL, 0UL);
  // Creating PIM Fused Program
  prog.add(pimLT,fuse_root, fuse_expr_2 , fuse_expr_0);
  // Emitting Copy Device to Host
  prog.add(pimCopyDeviceToHost,fuse_expr_0,(void*)dst.data(), 0UL, 0UL);
  pimFuse(prog);
  // Emitting Deallocations
  pimFree(fuse_expr_0);
  pimFree(fuse_root);
  pimFree(fuse_expr_2);
} 

int main(int argc, char *argv[])
{
  struct Params params = getInputParams(argc, argv);
  std::cout << "Running GEMV for matrix row: " << params.row << " column: " << params.column << " and vector of size: " << params.column << std::endl;

  std::vector<int> srcVector (params.column, 1);
  std::vector<int> resultVector(params.row, 0); // Size resultVector to match expected output size
  std::vector<std::vector<int>> srcMatrix (params.column, std::vector<int>(params.row, 1)); // matrix should lay out in colXrow format for bitserial PIM

  if (params.shouldVerify) {
    if (params.inputFile == nullptr)
    { 
      getVector(params.column, srcVector);
      getMatrix(params.column, params.row, 0, srcMatrix);
    }
    else
    {
      std::cout << "Reading from input file is not implemented yet." << std::endl;
      return 1;
    }
  }

  if (!createDevice(params.configFile))
  {
    return 1;
  }

  // TODO: Check if vector can fit in one iteration. Otherwise need to run in multiple iteration.
  gemv(params.row, params.column, srcVector, srcMatrix, resultVector);

  if (params.shouldVerify)
  {
    bool shouldBreak = false; // shared flag variable

    // verify result
    #pragma omp parallel for
    for (size_t i = 0; i < params.row; ++i)
    {
      if (shouldBreak) continue;
      int result = 0;
      for (size_t j = 0; j < params.column; ++j)
      {
        result += srcMatrix[j][i] * srcVector[j];
      }
      if (result != resultVector[i])
      {
        #pragma omp critical
        {
          if (!shouldBreak)
          { // check the flag again in a critical section
            std::cout << "Wrong answer: " << resultVector[i] << " (expected " << result << ")" << std::endl;
            shouldBreak = true; // set the flag to true
          }
        }
      }
    }

    if (!shouldBreak) {
      std::cout << "\n\nCorrect Answer!!\n\n";
    }
  }

  pimShowStats();

  return 0;
}
