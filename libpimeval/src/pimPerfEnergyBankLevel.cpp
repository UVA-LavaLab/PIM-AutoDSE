// File: pimPerfEnergyBankLevel.cc
// PIMeval Simulator - Performance Energy Models
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "pimPerfEnergyBankLevel.h"
#include "pimCmd.h"
#include <cstdio>
#include <unordered_map>
#include <cmath>
#include <list>
#include <algorithm>
#include <unordered_set>
#include <iomanip>
#include <cstddef>

//! @brief  Perf energy model of bank-level PIM for func1
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForFunc1(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  uint64_t totalAct = 0;
  uint64_t totalPre = 0;
  uint64_t totalCAS = 0;
  uint64_t totalOp = 0;
  uint64_t totalL = 0;
  if (cmdType == PimCmdEnum::CONVERT_TYPE) {
    // for type conversion, ALU parallelism is determined by the wider data type
    bitsPerElement = std::max(bitsPerElement, objDest.getBitsPerElement(PimBitWidth::ACTUAL));
  }
  unsigned numCores = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();

  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  double numberOfOperationPerElement = ((double)bitsPerElement / m_blimpCoreBitWidth);
  unsigned minElementPerRegion = obj.isLoadBalanced() ? (std::ceil(obj.getNumElements() * 1.0 / numCores) - (maxElementsPerRegion * (numPass - 1))) : maxElementsPerRegion;
  // How many iteration require to read / write max elements per region
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned numBankPerChip = numCores / m_numChipsPerRank;
  double activateMS = minGDLItr * m_tGDL < m_tRAS * m_tCK ? m_tRAS * m_tCK : m_tACT; // Use tRAS if GDL is less than tRAS
  // for scalar operations an extra read is required to read the scalar value
  switch (cmdType)
  {
    case PimCmdEnum::COPY_O2O:
    {
      msRead = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = 0;
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = numPass * numCores * (m_eACT + m_ePRE) * 2;
      mjEnergy += ((m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks) + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += ((m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks) + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalAct = 2 * numPass;
      totalPre = 2 * numPass;
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 2;
      totalL = 0;
      break;
    }
    case PimCmdEnum::POPCOUNT:
    case PimCmdEnum::ABS:
    case PimCmdEnum::BIT_SLICE_EXTRACT:
    case PimCmdEnum::BIT_SLICE_INSERT:
    case PimCmdEnum::CONVERT_TYPE:
    {
      if (cmdType == PimCmdEnum::BIT_SLICE_EXTRACT) {
        // Assume on ALU cycle to do this for now
        // numberOfOperationPerElement *= 2; // 1 shift, 1 and
      } else if (cmdType == PimCmdEnum::BIT_SLICE_INSERT) {
        // Assume on ALU cycle to do this for now
        // numberOfOperationPerElement *= 5; // 2 shifts, 1 not, 1 and, 1 or
      }
      // Refer to fulcrum documentation
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + (activateMS + m_tPRE);
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = ((m_eACT + m_ePRE) * 2 + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += ((m_eACT + m_ePRE) * 2 + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      totalAct = 2 * numPass;
      totalPre = 2 * numPass; 
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 2;
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr);
      break;
    }
    case PimCmdEnum::ADD_SCALAR:
    case PimCmdEnum::SUB_SCALAR:
    case PimCmdEnum::MUL_SCALAR:
    case PimCmdEnum::DIV_SCALAR:
    {
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + (activateMS + m_tPRE) + m_tR + m_tGDL;
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = ((m_eACT + m_ePRE) * 2 + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += ((m_eACT + m_ePRE) * 2 + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks)) + (m_eAP * numCores + m_eR_L * numBankPerChip * m_numRanks);
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      totalAct = 2 * numPass + 1; // extra activate for scalar
      totalPre = 2 * numPass + 1; // extra precharge for scalar
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 2 + 1; // extra read for scalar
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr); // extra load for scalar
      break;
    }
    case PimCmdEnum::AND_SCALAR:
    case PimCmdEnum::OR_SCALAR:
    case PimCmdEnum::XOR_SCALAR:
    case PimCmdEnum::XNOR_SCALAR:
    case PimCmdEnum::GT_SCALAR:
    case PimCmdEnum::LT_SCALAR:
    case PimCmdEnum::EQ_SCALAR:
    case PimCmdEnum::NE_SCALAR:
    case PimCmdEnum::MIN_SCALAR:
    case PimCmdEnum::MAX_SCALAR:
    {
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + m_tR + m_tGDL + activateMS + m_tPRE;
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 2) +  (maxElementsPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 2) + (minElementPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks)) + (m_eAP * numCores + m_eR_L * numBankPerChip * m_numRanks);
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      totalAct = 2 * numPass + 1;
      totalPre = 2 * numPass + 1;
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 2 + 1; // extra read for scalar
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr); // extra load for scalar
      break;
    }
    case PimCmdEnum::SHIFT_BITS_L:
    case PimCmdEnum::SHIFT_BITS_R:
    case PimCmdEnum::NOT:
    {
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + (activateMS + m_tPRE);
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 2) +  (maxElementsPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 2) + (minElementPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      totalAct = 2 * numPass;
      totalPre = 2 * numPass;
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 2;
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr);
      break;
    }
    case PimCmdEnum::AES_SBOX:
    case PimCmdEnum::AES_INVERSE_SBOX:
    {
      // NOTE:
      // Although the Processing Element (PE) is 32 bits wide and can theoretically perform four 8-bit operations in parallel,
      // in the case of these LUT-based commands (e.g., AES S-box or inverse S-box), each operation is treated as a single,
      // independent access driven by an 8-bit input.
      //
      // If the operation instead made full use of the 32-bit PE width to process four 8-bit inputs in parallel
      // then numberOfOperationPerElement would be 0.25. However, such parallelism is not modeled here due to the limitation of the LUT.
      // Therefore, for the uint8 data type, we set numberOfOperationPerElement = 1 because each 8-bit input
      // corresponds to one logical LUT access, and we assume that this access is not vectorized across multiple inputs
      // within a single PE execution. In other words, we model the cost at the granularity of one element per operation.
      numberOfOperationPerElement = 1;
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + (activateMS + m_tPRE);
      msWrite = ((m_tACT + m_tPRE + maxGDLItr * m_tGDL) * (numPass - 1)) + (activateMS + m_tPRE + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = ((m_eAP * 2) +  (maxElementsPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += ((m_eAP * 2) + (minElementPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      break;
    }
    default:
      printf("PIM-Warning: Perf energy model not available for PIM command %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp, totalAct, totalPre, totalCAS, totalL);
}

//! @brief  Perf energy model of bank-level PIM for func2
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForFunc2(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objSrc2, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned numCoresUsed = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
  uint64_t totalAct = 0;
  uint64_t totalPre = 0;
  uint64_t totalCAS = 0;
  uint64_t totalL = 0;
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  double numberOfOperationPerElement = ((double)bitsPerElement / m_blimpCoreBitWidth);
  unsigned minElementPerRegion = obj.isLoadBalanced() ? (std::ceil(obj.getNumElements() * 1.0 / numCoresUsed) - (maxElementsPerRegion * (numPass - 1))) : maxElementsPerRegion;
  // How many iteration require to read / write max elements per region
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  uint64_t totalOp = 0;
  unsigned numBankPerChip = numCoresUsed / m_numChipsPerRank;
  double activateMS = minGDLItr * m_tGDL < m_tRAS * m_tCK ? m_tRAS * m_tCK : m_tACT; // Use tRAS if GDL is less than tRAS

  switch (cmdType)
  {
    case PimCmdEnum::ADD:
    case PimCmdEnum::SUB:
    case PimCmdEnum::MUL:
    case PimCmdEnum::DIV:
    {
      totalAct = 3 * numPass;
      totalPre = 3 * numPass;
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 3; // 2 CAS per GDL iteration (1 read, 1 write)
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr);
      msRead = ((2 * (m_tACT + m_tPRE)) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((2 * (activateMS + m_tPRE)) + (minGDLItr * m_tGDL));
      msWrite = ((m_tACT + m_tPRE) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((activateMS + m_tPRE) + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 3) + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCoresUsed * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 3) + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCoresUsed;
      mjEnergy += ((m_eR_L * 2 * maxGDLItr * (numPass-1)) + (m_eR_L * 2 * minGDLItr)) * numBankPerChip * m_numRanks;
      mjEnergy += ((m_eW_L * maxGDLItr * (numPass-1)) + (m_eW_L * minGDLItr)) * numBankPerChip * m_numRanks;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      break;
    }
    case PimCmdEnum::SCALED_ADD:
    {
      /**
       * Performs a multiply-add operation on rows in DRAM.
       *
       * This command executes the following steps:
       * 1. Multiply the elements of a source row by a scalar value.
       * 2. Add the result of the multiplication to the elements of another row.
       * 3. Write the final result back to a row in DRAM.
       *
       * Performance Optimizations:
       * - While performing the multiplication, the next row to be added can be fetched without any additional overhead.
       * - During the addition, the next row to be multiplied can be fetched concurrently.
       *
       * As a result, only one read operation is necessary for the entire pass.
      */

      totalAct = 3 * numPass + 1; // extra activate for the first read of the first multiply
      totalPre = 3 * numPass + 1; // extra precharge for the first read of the first multiply
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 3; // 2 CAS per GDL iteration (1 read, 1 write)
      msRead = ((m_tACT + m_tPRE) * 2) * (numPass - 1) + (m_tR + m_tGDL) + (activateMS + m_tPRE);
      msWrite = ((m_tACT + m_tPRE) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((activateMS + m_tPRE) + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * 2 * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement * 2);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 3) + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement * 2)) * numCoresUsed * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 3) + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement * 2)) * numCoresUsed;
      mjEnergy += ((m_eR_L * 2 * maxGDLItr * (numPass-1)) + (m_eR_L * 2 * minGDLItr)) * numBankPerChip * m_numRanks + (m_eAP * numCoresUsed + m_eR_L * numBankPerChip * m_numRanks);
      mjEnergy += ((m_eW_L * maxGDLItr * (numPass-1)) + (m_eW_L * minGDLItr)) * numBankPerChip * m_numRanks;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements() * 2;
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr) * 2;
      break;
    }
    case PimCmdEnum::AND:
    case PimCmdEnum::OR:
    case PimCmdEnum::XOR:
    case PimCmdEnum::XNOR:
    case PimCmdEnum::GT:
    case PimCmdEnum::LT:
    case PimCmdEnum::EQ:
    case PimCmdEnum::NE:
    case PimCmdEnum::MIN:
    case PimCmdEnum::MAX:
    case PimCmdEnum::COND_BROADCAST:
    case PimCmdEnum::COND_SELECT:
    case PimCmdEnum::COND_SELECT_SCALAR:
    {

      totalAct = 3 * numPass;
      totalPre = 3 * numPass;
      totalCAS = (maxGDLItr * (numPass - 1) + minGDLItr) * 3; // 2 CAS per GDL iteration (1 read, 1 write)
      msRead = ((2 * (m_tACT + m_tPRE)) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((2 * (activateMS + m_tPRE)) + (minGDLItr * m_tGDL));
      msWrite = ((m_tACT + m_tPRE) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((activateMS + m_tPRE) + (minGDLItr * m_tGDL));
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 3) + (maxElementsPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCoresUsed * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 3) + (minElementPerRegion * m_blimpLogicalEnergy * numberOfOperationPerElement)) * numCoresUsed;
      mjEnergy += ((m_eR_L * 2 * maxGDLItr * (numPass-1)) + (m_eR_L * 2 * minGDLItr)) * numBankPerChip * m_numRanks;
      mjEnergy += ((m_eW_L * maxGDLItr * (numPass-1)) + (m_eW_L * minGDLItr)) * numBankPerChip * m_numRanks;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      totalL = (maxGDLItr * (numPass - 1) + minGDLItr) * 2;
      break;
    }
    default:
      printf("PIM-Warning: Perf energy model not available for PIM command %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp, totalAct, totalPre, totalCAS, totalL);
}

//! @brief  Perf energy model of bank-level PIM for reduction sum
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForReduction(PimCmdEnum cmdType, const pimObjInfo& obj, unsigned numPass) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned numCore = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
  double cpuTDP = 225; // W; AMD EPYC 9124 16 core
  unsigned minElementPerRegion = obj.isLoadBalanced() ? (std::ceil(obj.getNumElements() * 1.0 / numCore) - (maxElementsPerRegion * (numPass - 1))) : maxElementsPerRegion;
  // How many iteration require to read / write max elements per region
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  uint64_t totalOp = 0;
  unsigned numBankPerChip = numCore / m_numChipsPerRank;
  double activateMS = minGDLItr * m_tGDL < m_tRAS * m_tCK ? m_tRAS * m_tCK : m_tACT; // Use tRAS if GDL is less than tRAS
  switch (cmdType) {
    case PimCmdEnum::REDSUM:
    case PimCmdEnum::REDSUM_RANGE:
    case PimCmdEnum::REDMIN:
    case PimCmdEnum::REDMIN_RANGE:
    case PimCmdEnum::REDMAX:
    case PimCmdEnum::REDMAX_RANGE:
    {
      // How many iteration require to read / write max elements per region
      double numberOfOperationPerElement = ((double)bitsPerElement / m_blimpCoreBitWidth);
      msRead = (m_tACT + m_tPRE) * (numPass - 1) + (activateMS + m_tPRE);
      // reduction for all regions assuming 16 core AMD EPYC 9124
      double aggregateMs = static_cast<double>(obj.getNumCoresUsed()) / 2300000;
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement) + aggregateMs;
      msRuntime = msRead + msWrite + msCompute;

      // Refer to fulcrum documentation
      mjEnergy = ((m_eACT + m_ePRE) + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * (numPass - 1) * numCore;
      mjEnergy += ((m_eACT + m_ePRE) + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCore;
      mjEnergy += aggregateMs * cpuTDP;
      mjEnergy += ((m_eR_L * maxGDLItr * (numPass-1)) + (m_eR_L * minGDLItr)) * numBankPerChip;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      
      break;
    }
    default:
      printf("PIM-Warning: Unsupported reduction command for bank-level PIM: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
    }

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp, numPass, numPass, (minGDLItr + maxGDLItr * numPass), (minGDLItr + maxGDLItr * numPass));
}

//! @brief  Perf energy model of bank-level PIM for broadcast
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForBroadcast(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned numCore = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned minElementPerRegion = obj.isLoadBalanced() ? (std::ceil(obj.getNumElements() * 1.0 / obj.getNumCoreAvailable()) - (maxElementsPerRegion * (numPass - 1))) : maxElementsPerRegion;
  // How many iteration require to read / write max elements per region
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned numBankPerChip = numCore / m_numChipsPerRank;
  double activateMS = minGDLItr * m_tGDL < m_tRAS * m_tCK ? m_tRAS * m_tCK : m_tACT; // Use tRAS if GDL is less than tRAS
  uint64_t totalOp = 0;
  msWrite = ((m_tACT + m_tPRE) + (maxGDLItr * m_tGDL)) * (numPass - 1) + ((activateMS + m_tPRE) + (minGDLItr * m_tGDL));

  msRuntime = msRead + msWrite + msCompute;
  mjEnergy = (m_eACT + m_ePRE) * numPass * numCore;
  mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) + m_eW_L * minGDLItr) * numBankPerChip;
  mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp, numPass, numPass, (minGDLItr + maxGDLItr * numPass), 0);
}

// TODO: This needs to be revisited
//! @brief  Perf energy model of bank-level PIM for rotate
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForRotate(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned numRegions = obj.getRegions().size();
  uint64_t totalOp = 0;
  // boundary handling - assume two times copying between device and host for boundary elements
  pimeval::perfEnergy perfEnergyBT = getPerfEnergyForBytesTransfer(PimCmdEnum::COPY_D2H, numRegions * bitsPerElement / 8);

  // rotate within subarray:
  // For every bit: Read row to SA; move SA to R1; Shift R1 by N steps; Move R1 to SA; Write SA to row
  // TODO: separate bank level and GDL
  // TODO: energy unimplemented
  // TODO: perf per watt
  msRuntime = (m_tR + (bitsPerElement + 2) * m_tL + m_tW); // for one pass
  msRuntime *= numPass;
  mjEnergy = (m_eAP + (bitsPerElement + 2) * m_eL) * numPass;
  msRuntime += 2 * perfEnergyBT.m_msRuntime;
  mjEnergy += 2 * perfEnergyBT.m_mjEnergy;
  printf("PIM-Warning: Perf energy model is not precise for PIM command %s\n", pimCmd::getName(cmdType, "").c_str());

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of bank-level PIM for prefix-sum
pimeval::perfEnergy
pimPerfEnergyBankLevel::getPerfEnergyForPrefixSum(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned numCore = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
  double cpuTDP = 225; // W; AMD EPYC 9124 16 core
  unsigned minElementPerRegion = obj.isLoadBalanced() ? (std::ceil(obj.getNumElements() * 1.0 / numCore) - (maxElementsPerRegion * (numPass - 1))) : maxElementsPerRegion;
  // How many iteration require to read / write max elements per region
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  uint64_t totalOp = 0;
  unsigned numBankPerChip = numCore / m_numChipsPerRank;
  double activateMS = minGDLItr * m_tGDL < m_tRAS * m_tCK ? m_tRAS * m_tCK : m_tACT; // Use tRAS if GDL is less than tRAS
  switch (cmdType) {
    case PimCmdEnum::PREFIX_SUM:
    {
      /**
       * Performs prefix sum: dstVec[i] = dstVec[i-1] + srcVec[i]
       *
       * Execution Steps:
       * 1. Each bank performs a local prefix sum on its portion of the data.
       * 2. The host CPU fetches the final value from each subarray using `n`
       * DRAM READ. Here, `n = number of banks`.
       * 3. The host CPU aggregates these values (i.e., computes the prefix sum
       * across banks).
       * 4. The host CPU writes the aggregated values back to DRAM using `n`
       * DRAM WRITE.
       * 5. Each bank updates its elements using the received value to complete
       * the final prefix sum.
       *
       * Performance Model:
       * - While performing addition, the next row can be
       * fetched concurrently. As a result, `msRead = 2 * m_tR` (multiplied by
       * two because, two prefix sum iterations are required).
       * - `aggregateMs` models the time for host-side aggregation.
       * - `hostRW` accounts for host read/write overhead, including DRAM tR,
       * tW, and GDL delays.
       *
       */

      // How many iteration require to read / write max elements per region
      double numberOfOperationPerElement = ((double)bitsPerElement / m_blimpCoreBitWidth);
      msRead = (2 * numPass - 1) * (m_tACT + m_tPRE) + 2 * (activateMS + m_tPRE);
      msWrite = (2 * numPass - 1) * (m_tACT + m_tPRE) + 2 *(activateMS + m_tPRE);

      // reduction for all regions assuming 16 core AMD EPYC 9124
      double aggregateMs = static_cast<double>(obj.getNumCoresUsed()) / 2300000;
      double hostRW = (obj.getNumCoresUsed() * 1.0 / m_numChipsPerRank) * (m_tR + m_tW + (m_tGDL * 2));
      
      msCompute = (maxElementsPerRegion * m_blimpLatency * numberOfOperationPerElement * (numPass - 1)) + (minElementPerRegion * m_blimpLatency * numberOfOperationPerElement) + aggregateMs + hostRW;
      msRuntime = msRead + msWrite + msCompute;

      // Refer to fulcrum documentation
      mjEnergy = ((m_eACT + m_ePRE) + (maxElementsPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * (numPass - 1) * numCore * 2;
      mjEnergy += ((m_eACT + m_ePRE) + (minElementPerRegion * m_blimpArithmeticEnergy * numberOfOperationPerElement)) * numCore * 2;
      mjEnergy += aggregateMs * cpuTDP + ((obj.getNumCoresUsed() * 1.0 / m_numChipsPerRank) * ((2 * m_eAP)  + m_eR_L + m_eW_L));
      mjEnergy += ((m_eR_L * maxGDLItr * (numPass-1)) + (m_eR_L * minGDLItr)) * numBankPerChip * m_numRanks * 2;
      mjEnergy += ((m_eW_L * maxGDLItr * (numPass-1)) + (m_eW_L * minGDLItr)) * numBankPerChip * m_numRanks * 2;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements() * 2;
      break;
    }
    default:
      printf("PIM-Warning: Unsupported reduction command for bank-level PIM: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
    }
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

static std::string toString(pimeval::EventType type) {
    switch (type) {
        case pimeval::EventType::ACTIVATE_READ: return "ACTIVATE_READ";
        case pimeval::EventType::ACTIVATE_WRITE: return "ACTIVATE_WRITE";
        case pimeval::EventType::READ_SRC1: return "READ_SRC1";
        case pimeval::EventType::READ_SRC2: return "READ_SRC2";
        case pimeval::EventType::COMPUTE_CHUNK: return "COMPUTE";
        case pimeval::EventType::WRITE_CHUNK: return "WRITE_CHUNK";
        case pimeval::EventType::PRECHARGE_READ: return "PRECHARGE_READ";
        case pimeval::EventType::PRECHARGE_WRITE: return "PRECHARGE_WRITE";
        case pimeval::EventType::READ_SCALAR: return "READ_SCALAR";
        default: return "UNKNOWN";
    }
}


static std::vector<pimeval::EventNode*> createSortedEventList(
    const std::vector<pimeval::cmdNode>& cmdGraph, 
    bool memoryOnly = false, bool computeOnly = false) 
{
    std::vector<pimeval::EventNode*> allEvents;

    // Collect events from all commands
    for (const auto& cmd : cmdGraph) {
      for (const auto& [passId, eventList] : cmd.events) {
        for (auto elem : eventList) {
          for (auto* ev : elem.second) {
            // If filtering memory ops only
            if (memoryOnly) {
                if (ev->type == pimeval::EventType::ACTIVATE_READ ||
                    ev->type == pimeval::EventType::PRECHARGE_READ ||
                    ev->type == pimeval::EventType::ACTIVATE_WRITE ||
                    ev->type == pimeval::EventType::PRECHARGE_WRITE ||
                    ev->type == pimeval::EventType::READ_SRC1 ||
                    ev->type == pimeval::EventType::READ_SRC2 ||
                    ev->type == pimeval::EventType::READ_SCALAR ||
                    ev->type == pimeval::EventType::WRITE_CHUNK) 
                {
                    allEvents.push_back(ev);
                }
            } else if (computeOnly) {
                // If filtering compute ops only
                if (ev->type == pimeval::EventType::COMPUTE_CHUNK) {
                    allEvents.push_back(ev);
                }
            } else {
                allEvents.push_back(ev);
            }
          }
        }
      }
    }

    // Sort events by PassID → ChunkID → Topo order (eventID)
    std::unordered_map<uint64_t, int> inDegree;
    std::unordered_map<uint64_t, pimeval::EventNode*> idToEvent;

    for (auto* ev : allEvents) {
        idToEvent[ev->eventID] = ev;
        inDegree[ev->eventID] = ev->producers.size();
    }

    for (auto* ev : allEvents) {
      for (auto producerId : ev->producers) {
        if (!idToEvent.count(producerId->eventID)) {
          inDegree[ev->eventID]--;
        }
      }
    }

    // Priority queue for ready nodes with program-order tie-break
    auto cmp = [](const pimeval::EventNode* a, const pimeval::EventNode* b) {
        if (a->passId != b->passId) return a->passId > b->passId;
        return a->eventID > b->eventID;
    };

    std::priority_queue<
        pimeval::EventNode*, 
        std::vector<pimeval::EventNode*>, 
        decltype(cmp)
    > readyQueue(cmp);

    for (auto* ev : allEvents) {
        if (inDegree[ev->eventID] == 0) {
            readyQueue.push(ev);
        }
    }

    std::vector<pimeval::EventNode*> sortedEvents;
    while (!readyQueue.empty()) {
        auto* ev = readyQueue.top();
        readyQueue.pop();
        sortedEvents.push_back(ev);

        for (auto consumerId : ev->consumers) {
          if (--inDegree[consumerId->eventID] == 0) {
              readyQueue.push(idToEvent[consumerId->eventID]);
          }
        }
    }

    if (sortedEvents.size() != allEvents.size()) {
        printf("[ERROR] Cycle detected in global event graph!\n");
    }

    return sortedEvents;
}

void
pimPerfEnergyBankLevel::simulateExecution(std::vector<pimeval::cmdNode>& cmdGraph, std::vector<pimeval::perfEnergy> &perfEnergies) const {
  std::unordered_map<size_t, std::vector<pimeval::EventNode*>> cmdMap;
  unsigned numBanksPerChip = 1;
  unsigned numCores = 1;

  auto formatEvent = [&](pimeval::EventNode* e) -> std::string {
    std::stringstream ss;
    ss << "EventID: " << e->eventID
      << ", CmdID: " << e->cmdID
      << ", Cmd Type: " << pimCmd::getName(cmdGraph[e->cmdID].cmdType, "")
      << ", PassID: " << e->passId
      << ", ChunkID: " << e->chunkID
      << ", Type: " << toString(e->type);
    return ss.str();
  };

  std::function<void(pimeval::EventNode*, const std::unordered_map<int, std::vector<pimeval::EventNode*>>&, const std::string&, bool)> printDAGRecursive;
  printDAGRecursive = [&](pimeval::EventNode* e,
                       const std::unordered_map<int, std::vector<pimeval::EventNode*>>& forwardEdges,
                       const std::string& prefix,
                       bool isLast) {
    printf("%s", prefix.c_str());
    printf("%s", isLast ? "└── " : "├── ");
    printf("%s\n", formatEvent(e).c_str());

    auto it = forwardEdges.find(e->eventID);
    if (it == forwardEdges.end()) return;

    const auto& children = it->second;
    for (size_t i = 0; i < children.size(); ++i) {
      printDAGRecursive(children[i], forwardEdges,
                        prefix + (isLast ? "    " : "│   "),
                        i == children.size() - 1);
    }
  };


  auto executeMemoryEvent = [&](pimeval::EventNode* ev, unsigned currCycle) -> int {
    // Simulate execution of a memory event
    int cycleRequired = 0;
    for (auto p : ev->producers) {
      if (!p->hasExecuted) return cycleRequired;
    }
    switch (ev->type)
    {
    case pimeval::EventType::ACTIVATE_READ:
    {  
      ev->cycleCount = m_tRCDRD;
      ev->energyConsumed = m_eACT * numCores;
      cycleRequired = m_tRCDRD;
      for (auto c : ev->consumers) {
        if (c->type == pimeval::EventType::PRECHARGE_READ || c->type == pimeval::EventType::PRECHARGE_WRITE) {
          c->earliestCycle = currCycle + m_tRAS;
        }
      }
      break;
    }
    case pimeval::EventType::ACTIVATE_WRITE:
    {  
      ev->cycleCount = m_tRCDWR;
      ev->energyConsumed = m_eACT * numCores;
      cycleRequired = m_tRCDWR;
      for (auto c : ev->consumers) {
        if (c->type == pimeval::EventType::PRECHARGE_READ || c->type == pimeval::EventType::PRECHARGE_WRITE) {
          c->earliestCycle = currCycle + m_tRAS;
        }
      }
      break;
    }
    case pimeval::EventType::PRECHARGE_READ:
    case pimeval::EventType::PRECHARGE_WRITE:
    {
      ev->energyConsumed = m_ePRE * numCores;
      ev->cycleCount = m_tRP + ev->stalledCycle;
      cycleRequired = m_tRP + ev->stalledCycle;
      break;
    }
    case pimeval::EventType::READ_SRC1:
    case pimeval::EventType::READ_SRC2:
    case pimeval::EventType::READ_SCALAR:
    {
      ev->cycleCount = m_tCCD_L;
      ev->energyConsumed = m_eR_L * numBanksPerChip * m_numRanks;
      cycleRequired = m_tCCD_L;
      break;
    }
    case pimeval::EventType::WRITE_CHUNK:
    {
      ev->cycleCount = m_tCCD_L;
      ev->energyConsumed = m_eW_L * numBanksPerChip * m_numRanks;
      cycleRequired = m_tCCD_L;
      break;
    }
    default:
      break;
    }
    return cycleRequired;
  };

  auto executeComputeEvent = [&](pimeval::EventNode* ev) -> int {
    // Simulate execution of a compute event
    int cycleRequired = 0;
    for (auto p : ev->producers) {
      if (!p->hasExecuted) return cycleRequired;
    }
    switch (cmdGraph[ev->cmdID].cmdType)
    {
    case PimCmdEnum::ABS:
    case PimCmdEnum::POPCOUNT:
    case PimCmdEnum::ADD_SCALAR:
    case PimCmdEnum::SUB_SCALAR:
    case PimCmdEnum::MUL_SCALAR:
    case PimCmdEnum::DIV_SCALAR:
    case PimCmdEnum::ADD:
    case PimCmdEnum::SUB:
    case PimCmdEnum::MUL:
    case PimCmdEnum::DIV:
    {
      double itr = (ev->bitsPerElement * 1.0 / 32);
      // printf("PIM-Info: Arithmetic operation with %d bits per element requires %f iterations\n", ev->bitsPerElement, itr);
      cycleRequired = std::ceil(m_tCCD_L * itr);
      ev->cycleCount = std::ceil(m_tCCD_L * itr);
      ev->energyConsumed = m_blimpArithmeticEnergy * numCores * itr;
      // printf("Compute EventID %lu (%s) requires %d cycles and consumes %f nJ\n", ev->eventID, pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "").c_str(), ev->cycleCount, ev->energyConsumed);
      break;
    }
    case PimCmdEnum::REDSUM:
    case PimCmdEnum::REDMIN:
    case PimCmdEnum::REDMAX:
    case PimCmdEnum::REDSUM_RANGE:
    case PimCmdEnum::REDMIN_RANGE:
    case PimCmdEnum::REDMAX_RANGE:
    {
      double itr = (ev->bitsPerElement * 1.0 / 32);
      cycleRequired = std::ceil(m_tCCD_L * itr);
      ev->cycleCount = std::ceil(m_tCCD_L * itr);
      ev->energyConsumed = m_blimpArithmeticEnergy * numCores * itr;
      break;
    }
    case PimCmdEnum::MIN:
    case PimCmdEnum::MAX:
    case PimCmdEnum::GT:
    case PimCmdEnum::LT:
    case PimCmdEnum::EQ:
    case PimCmdEnum::NE:
    case PimCmdEnum::MIN_SCALAR:
    case PimCmdEnum::MAX_SCALAR:
    case PimCmdEnum::GT_SCALAR:
    case PimCmdEnum::LT_SCALAR:
    case PimCmdEnum::EQ_SCALAR:
    case PimCmdEnum::NE_SCALAR:
    case PimCmdEnum::CONVERT_TYPE:
    case PimCmdEnum::AND_SCALAR:
    case PimCmdEnum::OR_SCALAR:
    case PimCmdEnum::XOR_SCALAR:
    case PimCmdEnum::XNOR_SCALAR:
    case PimCmdEnum::SHIFT_BITS_L:
    case PimCmdEnum::SHIFT_BITS_R:
    case PimCmdEnum::AND:
    case PimCmdEnum::OR:
    case PimCmdEnum::XOR:
    case PimCmdEnum::XNOR:
    case PimCmdEnum::NOT:
    case PimCmdEnum::COND_SELECT_SCALAR:
    case PimCmdEnum::COND_SELECT:
    case PimCmdEnum::BROADCAST:
    case PimCmdEnum::COND_BROADCAST:
    case PimCmdEnum::COPY_O2O:
    case PimCmdEnum::BIT_SLICE_EXTRACT:
    case PimCmdEnum::BIT_SLICE_INSERT:
    {
      double itr = (ev->bitsPerElement * 1.0 / 32);
      // printf("PIM-Info: Arithmetic operation with %d bits per element requires %.9f iterations\n", ev->bitsPerElement, itr);
      cycleRequired = std::ceil(m_tCCD_L * itr);
      ev->cycleCount = std::ceil(m_tCCD_L * itr);
      ev->energyConsumed = m_blimpLogicalEnergy * numCores * itr;
      // printf("Compute EventID %lu (%s) requires %d cycles and consumes %.9f nJ\n", ev->eventID, pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "").c_str(), ev->cycleCount, ev->energyConsumed);
      break;
    }
    case PimCmdEnum::SCALED_ADD:
    {
      double itr = (ev->bitsPerElement * 1.0 / 32);
      cycleRequired = std::ceil(m_tCCD_L * itr) * 2;
      ev->cycleCount = std::ceil(m_tCCD_L * itr) * 2;
      ev->energyConsumed = m_blimpArithmeticEnergy * numCores * itr * 2;
      break;
    }
    default:
    {
      printf("PIM-Warning: Perf energy model is not available for PIM command %s\n", pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "").c_str());
      cycleRequired = -1;
      ev->cycleCount = UINT_MAX;
      ev->energyConsumed = 999999999.99;
      break;
    }
    }
    return cycleRequired;
  };

  auto sortEventsInChunks = [&](pimeval::cmdNode& cmdNode) {

    for (auto& [passId, chunkEvents] : cmdNode.events) {
      for (auto elem : chunkEvents) {
        // Build maps for in-degree and ID lookup
        std::unordered_map<uint64_t, int> inDegree;
        std::unordered_map<uint64_t, pimeval::EventNode*> idToEvent;
        for (auto ev : elem.second) {
          inDegree[ev->eventID] = ev->producers.size();  
          idToEvent[ev->eventID] = ev;
        }

        for (auto ev : elem.second) {
          for (auto producerId : ev->producers) {
            if (!idToEvent.count(producerId->eventID)) inDegree[ev->eventID]--;
          }
        }

        // Queue for zero in-degree nodes
        std::queue<pimeval::EventNode*> readyQueue;
        for (auto ev : elem.second) {
          if (inDegree[ev->eventID] == 0) {
            readyQueue.push(ev);
          }
        }

        std::vector<pimeval::EventNode*> sortedEvents;

        while (!readyQueue.empty()) {
          auto* ev = readyQueue.front();
          readyQueue.pop();

          sortedEvents.push_back(ev);

          for (auto consumerId : ev->consumers) {
            if (inDegree.count(consumerId->eventID)) {
              inDegree[consumerId->eventID]--;
              if (inDegree[consumerId->eventID] == 0) {
                readyQueue.push(idToEvent[consumerId->eventID]);
              }
            }
          }
        }

        if (sortedEvents.size() != elem.second.size()) {
          printf("[ERROR] Cycle detected in PassID: %d\n", passId);
          printf("Event IDs in this chunk:\n");
          for (auto ev : elem.second) {
            printf("EventID: %lu EventType: %s Producers: ", ev->eventID, toString(ev->type).c_str());
            for (auto p : ev->producers) printf("%lu ", p->eventID);
            printf(" | Consumers: ");
            for (auto c : ev->consumers) printf("%lu ", c->eventID);
            printf("\n");
          }
        }
        cmdNode.events[passId][elem.first] = std::move(sortedEvents);
      }
    }
  };

  auto createInterCMDEdge = [&]() {
    for (auto& node : cmdGraph) {
      if ((node.cmdType == PimCmdEnum::BROADCAST || node.cmdType == PimCmdEnum::COND_BROADCAST) && node.events.empty()) {
        // Skip broadcast nodes without events
        continue;
      }
      if (node.cmdType == PimCmdEnum::COPY_H2D || node.cmdType == PimCmdEnum::COPY_D2H)
        continue;

      for (auto pID : node.producers) {
        auto& producerNode = cmdGraph[pID];
        if (producerNode.cmdType == PimCmdEnum::COPY_H2D || producerNode.cmdType == PimCmdEnum::COPY_D2H)
          continue;
        if ((producerNode.cmdType == PimCmdEnum::BROADCAST || producerNode.cmdType == PimCmdEnum::COND_BROADCAST) && producerNode.events.empty()) {
          // Skip broadcast nodes without events
          continue;
        }

        for (auto& [passId, chunkMap] : producerNode.events) {
          for (auto& [chunkId, producerEventList] : chunkMap) {
            // Find producer compute event
            pimeval::EventNode* producerEvent = nullptr;
            for (auto* e : producerEventList) {
              if (e->type == pimeval::EventType::COMPUTE_CHUNK) {
                producerEvent = e;
                break;
              }
            }

            if (!producerEvent) {
              printf("[WARN] No valid producer event for CmdID %zu CmdType: %s at pass %d, chunk %d\n",
                     node.cmdId, pimCmd::getName(node.cmdType, "").c_str(), passId, chunkId);
              continue;
            }

            // Find corresponding consumer compute event
            auto consumerPassIt = node.events.find(passId);
            if (consumerPassIt == node.events.end())
              continue;

            auto chunkIt = consumerPassIt->second.find(chunkId);
            if (chunkIt == consumerPassIt->second.end())
              continue;

            pimeval::EventNode* consumerEvent = nullptr;
            for (auto* e : chunkIt->second) {
              if (e->type == pimeval::EventType::COMPUTE_CHUNK) {
                consumerEvent = e;
                break;
              }
            }

            if (!consumerEvent) {
              printf("[WARN] No valid consumer event for CmdID %zu CmdType: %s at pass %d, chunk %d\n",
                     node.cmdId, pimCmd::getName(node.cmdType, "").c_str(), passId, chunkId);
              continue;
            }

            // Add edge
            producerEvent->consumers.push_back(consumerEvent);
            consumerEvent->producers.push_back(producerEvent);
          }
        }
      }
    }
  };


  auto createIntraCMDEdge = [&](size_t currentCmdId) {
    if (cmdMap.find(currentCmdId) == cmdMap.end()) {
      return;
    }
    // Group events by passId and chunkId
    std::unordered_map<unsigned, std::vector<pimeval::EventNode*>> passEvents;
    for (auto ev : cmdMap[currentCmdId]) {
      passEvents[ev->passId].push_back(ev);
    }
    // Process each pass (row) individually
    for (const auto& [passId, eventsInPass] : passEvents) {
      // Separate by type
      std::map<unsigned, std::vector<pimeval::EventNode*>> readActivatesByPass;
      std::map<unsigned, std::vector<pimeval::EventNode*>> writeActivatesByPass;
      std::map<unsigned, std::vector<pimeval::EventNode*>> readPrechargesByPass;
      std::map<unsigned, std::vector<pimeval::EventNode*>> writePrechargesByPass;
      std::map<unsigned, pimeval::EventNode*> read1ByChunk;
      std::map<unsigned, pimeval::EventNode*> read2ByChunk;
      std::map<unsigned, pimeval::EventNode*> computeByChunk;
      std::map<unsigned, pimeval::EventNode*> writeByChunk;
      bool readScalar = false;
      pimeval::EventNode* readScalarEvent = nullptr;
      unsigned maxChunkId = 0;
      // Categorize
      for (auto* ev : eventsInPass) {
        maxChunkId = std::max(maxChunkId, ev->chunkID);
        switch (ev->type) {
          case pimeval::EventType::ACTIVATE_READ:
            readActivatesByPass[ev->chunkID].push_back(ev);
            break;
          case pimeval::EventType::ACTIVATE_WRITE:
            writeActivatesByPass[ev->chunkID].push_back(ev);
            break;
          case pimeval::EventType::PRECHARGE_READ:
            readPrechargesByPass[ev->chunkID].push_back(ev);
            break;
          case pimeval::EventType::PRECHARGE_WRITE:
            writePrechargesByPass[ev->chunkID].push_back(ev);
            break;
          case pimeval::EventType::READ_SRC1:
            read1ByChunk[ev->chunkID] = ev;
            break;
          case pimeval::EventType::READ_SRC2:
            read2ByChunk[ev->chunkID] = ev;
            break;
          case pimeval::EventType::COMPUTE_CHUNK:
            computeByChunk[ev->chunkID] = ev;
            break;
          case pimeval::EventType::WRITE_CHUNK:
            writeByChunk[ev->chunkID] = ev;
            break;
          case pimeval::EventType::READ_SCALAR:
            readScalar = true;
            readScalarEvent = ev;
            break;
          default:
            break;
        }
      }
      pimeval::EventNode* lastPrecharge = nullptr;
      if (readScalar) {
        if (!readActivatesByPass[0].empty() && !readPrechargesByPass.empty() && !readPrechargesByPass.begin()->second.empty()) {
          auto* activate = readActivatesByPass[0][0];
          auto* precharge = readPrechargesByPass.begin()->second[0];

          activate->consumers.push_back(readScalarEvent);
          readScalarEvent->producers.push_back(activate);

          activate->consumers.push_back(precharge);
          precharge->producers.push_back(activate);

          readActivatesByPass[0].erase(readActivatesByPass[0].begin());
          if (readActivatesByPass[0].empty()) {
            readActivatesByPass.erase(0);
          }

          lastPrecharge = precharge;

          precharge->producers.push_back(readScalarEvent);
          readScalarEvent->consumers.push_back(precharge);

          readPrechargesByPass.begin()->second.erase(readPrechargesByPass.begin()->second.begin());
          if (readPrechargesByPass.begin()->second.empty()) {
            readPrechargesByPass.erase(readPrechargesByPass.begin());
          }
        }
      }

      for (unsigned c = 0; c <= maxChunkId; ++c) {
        // ----------- READ 1 ------------
        if (readActivatesByPass.count(c) && read1ByChunk.count(c)) {
          if (!readActivatesByPass[c].empty() && !readPrechargesByPass.empty() && !readPrechargesByPass.begin()->second.empty()) {
            auto* activate = readActivatesByPass[c][0];
            auto* precharge = readPrechargesByPass.begin()->second[0];
            auto* read = read1ByChunk[c];
            unsigned prechargeChunkId = precharge->chunkID;

            if (lastPrecharge) {
              // if (lastPrecharge->type == pimeval::EventType::PRECHARGE_WRITE) {
              //   printf("EventID: %lu, Type: %s is a producer of EventID: %lu, Type: %s\n", lastPrecharge->eventID, toString(lastPrecharge->type).c_str(), activate->eventID, toString(activate->type).c_str());
              // }
              lastPrecharge->consumers.push_back(activate);
              activate->producers.push_back(lastPrecharge);
            }

            activate->consumers.push_back(read);
            read->producers.push_back(activate);

            activate->consumers.push_back(precharge);
            precharge->producers.push_back(activate);
            if (read1ByChunk.count(prechargeChunkId)) {
              precharge->producers.push_back(read1ByChunk[prechargeChunkId]);
              read1ByChunk[prechargeChunkId]->consumers.push_back(precharge);
            }

            lastPrecharge = precharge;

            readActivatesByPass[c].erase(readActivatesByPass[c].begin());
            if (readActivatesByPass[c].empty()) {
              readActivatesByPass.erase(c);
            }

            readPrechargesByPass.begin()->second.erase(readPrechargesByPass.begin()->second.begin());
            if (readPrechargesByPass.begin()->second.empty()) {
              readPrechargesByPass.erase(readPrechargesByPass.begin());
            }
          }
        }

        // ----------- READ 2 ------------
        if (readActivatesByPass.count(c) && read2ByChunk.count(c)) {
          if (!readActivatesByPass[c].empty() && !readPrechargesByPass.empty() && !readPrechargesByPass.begin()->second.empty()) {
            auto* activate = readActivatesByPass[c][0];
            auto* precharge = readPrechargesByPass.begin()->second[0];
            auto* read = read2ByChunk[c];
            unsigned prechargeChunkId = precharge->chunkID;
            // if (c > 0) {
            //    printf("Activate: [ %s] | Read2:[ %s]\n", formatEvent(activate).c_str(), formatEvent(read).c_str());
            // }

            if (lastPrecharge) {
              lastPrecharge->consumers.push_back(activate);
              activate->producers.push_back(lastPrecharge);
            }

            activate->consumers.push_back(read);
            read->producers.push_back(activate);
            // if (c > 0) {
            //   //  printf("Activate: [ %s] | Read2:[ %s]\n", formatEvent(activate).c_str(), formatEvent(read).c_str());
            //   printf("Producers of Read2: %s\n", formatEvent(read).c_str());
            //   for (auto* p : read->producers) {
            //     if (p) printf("\n%s ", formatEvent(p).c_str());
            //   }
            //   printf("\n");
            // }

            activate->consumers.push_back(precharge);
            precharge->producers.push_back(activate);
            if (read2ByChunk.count(prechargeChunkId)) {
              precharge->producers.push_back(read2ByChunk[prechargeChunkId]);
              read2ByChunk[prechargeChunkId]->consumers.push_back(precharge);
            }

            lastPrecharge = precharge;

            readActivatesByPass[c].erase(readActivatesByPass[c].begin());
            if (readActivatesByPass[c].empty()) {
              readActivatesByPass.erase(c);
            }

            readPrechargesByPass.begin()->second.erase(readPrechargesByPass.begin()->second.begin());
            if (readPrechargesByPass.begin()->second.empty()) {
              readPrechargesByPass.erase(readPrechargesByPass.begin());
            }
          }
        }

        // ----------- WRITE ------------
        if (writeActivatesByPass.count(c) && writeByChunk.count(c)) {
          if (!writeActivatesByPass[c].empty() && !writePrechargesByPass.empty() && !writePrechargesByPass.begin()->second.empty()) {
            auto* activate = writeActivatesByPass[c][0];
            auto* precharge = writePrechargesByPass.begin()->second[0];
            auto* write = writeByChunk[c];
            unsigned prechargeChunkId = precharge->chunkID;

            if (lastPrecharge) {
              lastPrecharge->consumers.push_back(activate);
              activate->producers.push_back(lastPrecharge);
            }

            activate->consumers.push_back(write);
            write->producers.push_back(activate);

            activate->consumers.push_back(precharge);
            precharge->producers.push_back(activate);

            if (writeByChunk.count(prechargeChunkId)) {
              precharge->producers.push_back(writeByChunk[prechargeChunkId]);
              writeByChunk[prechargeChunkId]->consumers.push_back(precharge);
            }

            lastPrecharge = precharge;

            writeActivatesByPass[c].erase(writeActivatesByPass[c].begin());
            if (writeActivatesByPass[c].empty()) {
              writeActivatesByPass.erase(c);
            }

            writePrechargesByPass.begin()->second.erase(writePrechargesByPass.begin()->second.begin());
            if (writePrechargesByPass.begin()->second.empty()) {
              writePrechargesByPass.erase(writePrechargesByPass.begin());
            }
          }
        }

        if (read1ByChunk.count(c) && computeByChunk.count(c)) {
          read1ByChunk[c]->consumers.push_back(computeByChunk[c]);
          computeByChunk[c]->producers.push_back(read1ByChunk[c]);
        }
        if (read2ByChunk.count(c) && computeByChunk.count(c)) {
          read2ByChunk[c]->consumers.push_back(computeByChunk[c]);
          computeByChunk[c]->producers.push_back(read2ByChunk[c]);
        }
        // Connect compute events to write events
        if (computeByChunk.count(c) && writeByChunk.count(c)) {
          computeByChunk[c]->consumers.push_back(writeByChunk[c]);
          writeByChunk[c]->producers.push_back(computeByChunk[c]);
        }
        if (c < maxChunkId) {
          if (read1ByChunk.count(c) && read1ByChunk.count(c + 1)) {
            read1ByChunk[c]->consumers.push_back(read1ByChunk[c + 1]);
            read1ByChunk[c + 1]->producers.push_back(read1ByChunk[c]);
          }
          if (read2ByChunk.count(c) && read2ByChunk.count(c + 1)) {
            read2ByChunk[c]->consumers.push_back(read2ByChunk[c + 1]);
            read2ByChunk[c + 1]->producers.push_back(read2ByChunk[c]);
          }
          if (writeByChunk.count(c) && writeByChunk.count(c + 1))
          {
            writeByChunk[c]->consumers.push_back(writeByChunk[c + 1]);
            writeByChunk[c + 1]->producers.push_back(writeByChunk[c]);
          }
        }
        // lastPrecharge = nullptr; // Reset for next chunk
      }
    }
  };

  // If register is narrower than GDL, data must be processed in register-sized chunks
  unsigned effectiveChunkWidth = std::min((unsigned)m_GDLWidth, m_blimpRegisterBitWidth);
  uint64_t currEventId = 0;
  for (auto& node : cmdGraph) {
    pimObjInfo& obj = node.srcs.empty() ? *node.dests[0] : *node.srcs[0];
    uint64_t numPass = obj.getMaxNumRegionsPerCore();
    uint64_t bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
    numCores = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
    uint64_t maxElementsPerRegion = obj.getMaxElementsPerRegion();
    uint64_t totalElements = obj.getNumElements();
    uint64_t maxGdlItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / effectiveChunkWidth);
    numBanksPerChip = numCores / m_numChipsPerRank;

    // Calculate minElementsPerRegion safely to avoid underflow
    uint64_t elementsPerCore = std::ceil(totalElements * 1.0 / numCores);
    uint64_t elementsInMaxPasses = maxElementsPerRegion * (numPass - 1);
    uint64_t minElementsPerRegion = (elementsPerCore > elementsInMaxPasses) ?
                                    (elementsPerCore - elementsInMaxPasses) :
                                    maxElementsPerRegion;
    uint64_t minGdlItr = std::ceil(minElementsPerRegion * bitsPerElement * 1.0 / effectiveChunkWidth);
    unsigned R1 = node.numRead1 == 0 ? 1 : node.numRead1, R2 = node.numRead2 == 0 ? 1 : node.numRead2;
    unsigned W = node.numWrite == 0 ? 1 : node.numWrite;
    unsigned W_stride = std::ceil(maxGdlItr / W), R1_stride = std::ceil(maxGdlItr / R1), R2_stride = std::ceil(maxGdlItr / R2);
    // printf("Processing command ID: %zu Type: %s NumCores: %zu BitsPerElement: %zu MaxElementsPerRegion: %zu TotalElements: %zu\n",
    //        node.cmdId, pimCmd::getName(node.cmdType, "").c_str(), numCores, bitsPerElement, maxElementsPerRegion, totalElements);
    if (node.cmdType == PimCmdEnum::BROADCAST || node.cmdType == PimCmdEnum::COND_BROADCAST) {
      // std::printf("Broadcast command detected: %s Command ID: %zu Has Write: %d\n", pimCmd::getName(node.cmdType, "").c_str(), node.cmdId, node.numWrite);
      if (node.numWrite == 0) {
        // If no write, we only need to read once
        perfEnergies[node.cmdId].m_msCompute = m_tCCD_L * m_tCK;
        perfEnergies[node.cmdId].m_mjEnergy = m_blimpLogicalEnergy * numCores * bitsPerElement / 32;
        continue;
      }
      for (uint64_t p = 0; p < numPass; ++p) {
        uint64_t totalChunks = p < numPass - 1 ? maxGdlItr : minGdlItr;
        for (uint64_t c = 0; c < totalChunks; ++c) {
          if (node.numWrite > 0) {
            if (c == 0) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
            }
            if (!node.dests.empty() && node.numWrite > 0 && c == totalChunks - 1) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
            }
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::WRITE_CHUNK, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
          }
          else {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::COMPUTE_CHUNK, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
          }
        }
      }
    } else if (node.cmdType == PimCmdEnum::COPY_H2D || node.cmdType == PimCmdEnum::COPY_D2H || node.cmdType == PimCmdEnum::COPY_D2D || node.cmdType == PimCmdEnum::COND_COPY) {
      continue;
    } else {
      for (uint64_t p = 0; p < numPass; ++p) {
        if (p == 0 && (node.cmdType == PimCmdEnum::ADD_SCALAR || node.cmdType == PimCmdEnum::SUB_SCALAR ||
            node.cmdType == PimCmdEnum::MUL_SCALAR || node.cmdType == PimCmdEnum::DIV_SCALAR ||
            node.cmdType == PimCmdEnum::MIN_SCALAR || node.cmdType == PimCmdEnum::MAX_SCALAR ||
            node.cmdType == PimCmdEnum::GT_SCALAR || node.cmdType == PimCmdEnum::LT_SCALAR ||
            node.cmdType == PimCmdEnum::EQ_SCALAR || node.cmdType == PimCmdEnum::NE_SCALAR ||
            node.cmdType == PimCmdEnum::AND_SCALAR || node.cmdType == PimCmdEnum::OR_SCALAR ||
            node.cmdType == PimCmdEnum::XOR_SCALAR || node.cmdType == PimCmdEnum::XNOR_SCALAR || 
            node.cmdType == PimCmdEnum::SCALED_ADD)) {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_READ, currEventId++, node.cmdId, 0, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][0].push_back(en);
            en = pimeval::generateEvent(pimeval::EventType::READ_SCALAR, currEventId++, node.cmdId, 0, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][0].push_back(en);
            en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_READ, currEventId++, node.cmdId, 0, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][0].push_back(en);
        }
        uint64_t totalChunks = p < numPass - 1 ? maxGdlItr : minGdlItr;
        unsigned writeACh = 0, writePCh = W_stride - 1, read1Ch = 0, read1PCh = R1_stride - 1, read2Ch = 0, read2PCh = R2_stride -1 ;

        for (uint64_t c = 0; c < totalChunks; ++c) {
          if (!node.dests.empty() && node.numWrite > 0 && c == writeACh)
          {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
            writeACh += W_stride;
          }
          if (node.srcs.size() > 0 && node.numRead1 > 0 && c == read1Ch) {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_READ, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
            read1Ch += R1_stride;
            if (node.srcs.size() == 2 && node.numRead2 > 0 && c == read2Ch) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_READ, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
              read2Ch += R2_stride;
            }
          }
          if (!node.dests.empty() && node.numWrite > 0 && (c == totalChunks - 1 || c == writePCh)) {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
            writePCh += W_stride;
          }
          if (node.srcs.size() > 0 && node.numRead1 > 0 && (c == totalChunks - 1 || c == read1PCh)) {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_READ, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
            read1PCh += R1_stride;
            if (node.srcs.size() == 2 && node.numRead2 > 0 && (c == totalChunks - 1 || c == read2PCh)) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_READ, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
              read2PCh += R2_stride;
            }
          }
          for (size_t s = 0; s < node.srcs.size(); ++s) {
            if (node.numRead1 <= s) break;
            pimeval::EventNode* en = pimeval::generateEvent(s == 0 ? pimeval::EventType::READ_SRC1 : pimeval::EventType::READ_SRC2, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
          } 
          for (size_t d = 0; d < node.dests.size() && node.numWrite > 0; ++d) {
            pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::WRITE_CHUNK, currEventId++, node.cmdId, c, p, bitsPerElement);
            cmdMap[node.cmdId].push_back(en);
            node.events[p][c].push_back(en);
          }
          pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::COMPUTE_CHUNK, currEventId++, node.cmdId, c, p, bitsPerElement);
          cmdMap[node.cmdId].push_back(en);
          node.events[p][c].push_back(en);
        }
      }
    }
    createIntraCMDEdge(node.cmdId);
    sortEventsInChunks(node);
  }
  createInterCMDEdge();
  
  // std::vector<pimeval::EventNode*> sortedEvents = createSortedEventList(cmdGraph, false, false);
  
  // // Build forwardEdges map
  // std::unordered_map<int, std::vector<pimeval::EventNode*>> forwardEdges;
  // std::unordered_map<int, pimeval::EventNode*> idToEvent;

  // for (auto* e : sortedEvents) {
  //   idToEvent[e->eventID] = e;
  //   for (auto* c : e->consumers) {
  //     forwardEdges[e->eventID].push_back(c);
  //   }
  // }

  // // Detect root events (no one lists them as consumers)
  // std::unordered_set<int> consumerIDs;
  // for (auto* e : sortedEvents) {
  //   for (auto* p : e->producers)
  //     consumerIDs.insert(e->eventID);
  // }

  // for (auto* e : sortedEvents) {
  //   if (consumerIDs.find(e->eventID) == consumerIDs.end()) {
  //     printDAGRecursive(e, forwardEdges, "", true);
  //   }
  // }

  // return;
  std::vector<pimeval::EventNode*> memoryEvents = createSortedEventList(cmdGraph, true, false);
  
  // Print with producers and consumers
  // printf("========= Sorted Memory Event List =========\n");
  // for (const auto* ev : memoryEvents) {
  //   printf("EventID: %lu, CmdID: %zu, Cmd Type: %s, PassID: %u, ChunkID: %u, Type: %s\n    Producers: ",
  //          ev->eventID, ev->cmdID, pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "").c_str(),
  //          ev->passId, ev->chunkID, toString(ev->type).c_str());
  //   if (ev->producers.empty()) {
  //     printf("None");
  //   } else {            
  //     for (auto pid : ev->producers) {
  //       printf("%lu ", pid->eventID);
  //     }
  //   }

  //   printf("\n    Consumers: ");
  //   if (ev->consumers.empty()) {
  //     printf("None");
  //   } else {
  //     for (auto cid : ev->consumers) {
  //         printf("%lu ", cid->eventID);
  //     }
  //   }
  //   printf("\n-------------------------------------\n");
  // }

  std::vector<pimeval::EventNode*> computeEvents = createSortedEventList(cmdGraph, false, true);
  
  // Print with producers and consumers
  // printf("\n\n========= Sorted Compute Event List =========\n");
  // for (const auto* ev : computeEvents) {
  //   printf("EventID: %lu, CmdID: %zu, Cmd Type: %s, PassID: %u, ChunkID: %u, Type: %s\n    Producers: ",
  //          ev->eventID, ev->cmdID, pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "").c_str(),
  //          ev->passId, ev->chunkID, toString(ev->type).c_str());
  //   if (ev->producers.empty()) {
  //           printf("None");
  //   } else {            
  //     for (auto pid : ev->producers) {
  //       printf("%lu ", pid->eventID);
  //     }
  //   }

  //   printf("\n    Consumers: ");
  //   if (ev->consumers.empty()) {
  //       printf("None");
  //   } else {
  //       for (auto cid : ev->consumers) {
  //           printf("%lu ", cid->eventID);
  //       }
  //   }
  //   printf("\n-------------------------------------\n");
  // }

  uint64_t clockCycle = 0, memReady = 0, compReady = 0;
  std::pair<pimeval::EventNode*, double> lastMemEv = {nullptr, 0.0};

  while (!memoryEvents.empty() || !computeEvents.empty()) {
    auto memEv = (!memoryEvents.empty() && clockCycle >= memReady) ? memoryEvents.front() : nullptr;
    auto compEv = (!computeEvents.empty() && clockCycle >= compReady) ? computeEvents.front() : nullptr;

    int memCycles = 0, compCycles = 0;
    bool hasCompute = false, hasMemory = false;
    if (memEv != nullptr) {
      bool hasProducersFinished = false;
      for (auto& producer : memEv->producers) {
        if (producer->hasExecuted) {
          hasProducersFinished = true;
        } else {
          hasProducersFinished = false;
          break;
        }
      }
      if (memEv->earliestCycle > clockCycle && hasProducersFinished) {
        memEv->stalledCycle = memEv->earliestCycle - clockCycle; // Advance clock if necessary
        clockCycle = memEv->earliestCycle;
      }
      switch (memEv->type)
      {
      case pimeval::EventType::ACTIVATE_READ:
      case pimeval::EventType::ACTIVATE_WRITE:
      {
          ++perfEnergies[memEv->cmdID].m_totalACT;
          break;
      }
      case pimeval::EventType::READ_SRC1:
      case pimeval::EventType::READ_SRC2:
      case pimeval::EventType::READ_SCALAR:
      case pimeval::EventType::WRITE_CHUNK:
      {
          ++perfEnergies[memEv->cmdID].m_totalCAS;
          break;
      }
      case pimeval::EventType::PRECHARGE_READ:
      case pimeval::EventType::PRECHARGE_WRITE:
      {
          ++perfEnergies[memEv->cmdID].m_totalPRE;
          break;
      }
      default:
      {
        std::printf("[ERROR] Invalid memory event type for EventID: %lu, CmdID: %zu, Type: %s\n",
                    memEv->eventID, memEv->cmdID, toString(memEv->type).c_str());
        break;
      }
      }
      memCycles = executeMemoryEvent(memEv, clockCycle);
      // std::printf("[Cycle %lu] Scheduling MEM EventID: %lu, CmdID: %zu, Type: %s, Duration: %d cycles, Ready at: %lu\n",
      //           clockCycle, memEv->eventID, memEv->cmdID, toString(memEv->type).c_str(), memCycles, memReady);
      if (memCycles != 0) {
        hasMemory = true;
        memReady = clockCycle + memCycles;
        memEv->hasExecuted = true; // Mark as executed
        if (lastMemEv.first != nullptr) {
          if (lastMemEv.first->type == pimeval::EventType::WRITE_CHUNK || lastMemEv.first->type == pimeval::EventType::ACTIVATE_WRITE || lastMemEv.first->type == pimeval::EventType::PRECHARGE_WRITE) {
            perfEnergies[lastMemEv.first->cmdID].m_msWrite += lastMemEv.second;
          } else {
            perfEnergies[lastMemEv.first->cmdID].m_msRead += lastMemEv.second;
          }
          perfEnergies[lastMemEv.first->cmdID].m_mjEnergy += lastMemEv.first->energyConsumed;
          // printf("Mem Event: CmdID %zu Type %s Duration %.9f cycles Energy %.9f mJ\n", lastMemEv.first->cmdID, toString(lastMemEv.first->type).c_str(), lastMemEv.second, lastMemEv.first->energyConsumed);
        }
        lastMemEv = {memEv, memCycles * m_tCK}; // Update last memory event
        memoryEvents.erase(memoryEvents.begin());
        // std::printf("Memory Event Executed!\n");
      }
    }

    if (compEv != nullptr) {
      compCycles = executeComputeEvent(compEv);
      if (compCycles == -1) {
        hasCompute = true;
        compReady = clockCycle;
        compEv->hasExecuted = true; // Mark as executed
        computeEvents.erase(computeEvents.begin());
        ++perfEnergies[compEv->cmdID].m_totalL;
      }
      if (compCycles > 0) {
        ++perfEnergies[compEv->cmdID].m_totalL;
        hasCompute = true;
        compReady = clockCycle + compCycles;
        compEv->hasExecuted = true; // Mark as executed
        computeEvents.erase(computeEvents.begin());
        perfEnergies[compEv->cmdID].m_msCompute += compCycles * m_tCK;
        perfEnergies[compEv->cmdID].m_mjEnergy += compEv->energyConsumed;
        if (lastMemEv.first != nullptr && lastMemEv.second > 0) {
          lastMemEv.second -= compCycles * m_tCK; // Adjust last memory event duration
          lastMemEv.second = std::max(lastMemEv.second, 0.0); // Ensure non-negative
          // printf("Comp Event: CmdID %zu Type %s Duration %.9f cycles Energy %.9f mJ\n", compEv->cmdID, toString(compEv->type).c_str(), compCycles * m_tCK, compEv->energyConsumed);
          // printf("Last Mem Event Adjusted: CmdID %zu Type %s Remaining Duration %.9f cycles\n",
          //        lastMemEv.first->cmdID, toString(lastMemEv.first->type).c_str(), lastMemEv.second);
          
        }
      }
    }

    // Advance to the next earliest time either memory or compute can execute
    if (memEv != nullptr && compEv != nullptr && hasCompute && hasMemory) {
      clockCycle = std::min(memReady, compReady);
    }
    else if (memEv != nullptr && hasMemory)
      clockCycle = memReady;
    else if (compEv != nullptr && hasCompute)
      clockCycle = compReady;
    else {
      clockCycle += 1;
    }
  }
  if (lastMemEv.first && lastMemEv.second > 0) {
    if (lastMemEv.first->type == pimeval::EventType::WRITE_CHUNK ||
        lastMemEv.first->type == pimeval::EventType::ACTIVATE_WRITE ||
        lastMemEv.first->type == pimeval::EventType::PRECHARGE_WRITE) {
      perfEnergies[lastMemEv.first->cmdID].m_msWrite += lastMemEv.second;
      perfEnergies[lastMemEv.first->cmdID].m_mjEnergy += lastMemEv.first->energyConsumed;
    } else {
      perfEnergies[lastMemEv.first->cmdID].m_msRead += lastMemEv.second;
      perfEnergies[lastMemEv.first->cmdID].m_mjEnergy += lastMemEv.first->energyConsumed;
    }
    // printf("Mem Event: CmdID %zu Type %s Duration %.9f cycles Energy %.9f mJ\n", lastMemEv.first->cmdID, toString(lastMemEv.first->type).c_str(), lastMemEv.second, lastMemEv.first->energyConsumed);
  }
  for (auto& c: cmdGraph) {
    if (c.cmdType == PimCmdEnum::COPY_H2D || c.cmdType == PimCmdEnum::COPY_D2H || c.cmdType == PimCmdEnum::COPY_D2D || c.cmdType == PimCmdEnum::COPY_O2O || c.cmdType == PimCmdEnum::COND_COPY) {
      continue; // Skip copy commands
    }
    if (c.cmdType == PimCmdEnum::SCALED_ADD) {
      perfEnergies[c.cmdId].m_totalOp = c.srcs.empty() ? 0 : c.srcs[0]->getNumElements() * 2;
    } else {
      perfEnergies[c.cmdId].m_totalOp = c.srcs.empty() ? 0 : c.srcs[0]->getNumElements();
    }
    perfEnergies[c.cmdId].m_msRuntime = perfEnergies[c.cmdId].m_msRead + perfEnergies[c.cmdId].m_msWrite + perfEnergies[c.cmdId].m_msCompute;
    perfEnergies[c.cmdId].m_mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * perfEnergies[c.cmdId].m_msRuntime;
    // printf("Cmd ID %zu (%s):\n", c.cmdId, pimCmd::getName(c.cmdType, "").c_str());
    // printf("  Total Op: %lu\n", perfEnergies[c.cmdId].m_totalOp);
    // printf("  Read: %.9f ms, Write: %.9f ms, Compute: %.9f ms, Runtime: %.9f ms, Energy: %.9f mJ\n",
    //        perfEnergies[c.cmdId].m_msRead, perfEnergies[c.cmdId].m_msWrite,
    //        perfEnergies[c.cmdId].m_msCompute, perfEnergies[c.cmdId].m_msRuntime,
    //        perfEnergies[c.cmdId].m_mjEnergy);
  }
}

//! @brief Perf energy model of bank-level PIM for PIM Prog
std::vector<pimeval::perfEnergy>
pimPerfEnergyBankLevel::getPerfEnergyForPIMProg(std::vector<pimeval::cmdNode>& cmdGraph) const {
  auto broadcastShouldWriteBack = [&](const pimeval::cmdNode& node) -> bool {
    for (auto& c : node.consumers) {
      if (cmdGraph[c].cmdType == PimCmdEnum::COPY_D2H) {
        return true; // If any consumer is a D2H copy, we should write back
      }
    }
    return false;
  };

  auto isAliveAfter = [&](const pimeval::cmdNode& node, size_t currCMD) -> bool {
    for (auto& c : node.consumers) {
      if (cmdGraph[c].cmdType == PimCmdEnum::COPY_D2H) {
        return true; // If any consumer is a D2H copy, we should write back
      }
      if (find(cmdGraph[c].srcs.begin(), cmdGraph[c].srcs.end(), node.dests[0]) != cmdGraph[c].srcs.end() && c > currCMD) {
        return true;
      }
    }
    return false;
  };
  
  std::vector<pimeval::perfEnergy> perfEnergies(cmdGraph.size(), pimeval::perfEnergy(0, 0, 0, 0, 0, 0, 0, 0, 0));
  std::unordered_map<PimObjId, bool> inVectorRegister;
  std::unordered_map<PimObjId, bool> inScalarRegister;
  std::unordered_map<PimObjId, std::pair<size_t, uint64_t>> shouldWriteBack;
  std::list<PimObjId> regLRU;
  unsigned usedReg = 0;

  uint64_t totalGRFbits = m_blimpRegisterCount * m_blimpRegisterBitWidth;
  // max allowable register bits per object should be what each operand of a 2 src 1 dest command can hold
  // assuming one of the srcs is latched 
  bool has2Src1Dest = false;
  unsigned mm = 0;
  unsigned bitsPerElement = 0;

  for (const auto& node : cmdGraph) {
    if (node.srcs.size() == 2 && node.dests.size() == 1) {
      has2Src1Dest = true;
      mm = node.srcs[0]->getBitsPerElement(PimBitWidth::ACTUAL) * node.srcs[0]->getMaxElementsPerRegion();
      bitsPerElement = std::max(bitsPerElement, node.srcs[0]->getBitsPerElement(PimBitWidth::ACTUAL));
    } else if ((node.srcs.size() == 1 && node.dests.size() == 1) ||
               (node.cmdType == PimCmdEnum::BROADCAST)) {
      mm = node.dests[0]->getBitsPerElement(PimBitWidth::ACTUAL) * node.dests[0]->getMaxElementsPerRegion();
      bitsPerElement = std::max(bitsPerElement, node.dests[0]->getBitsPerElement(PimBitWidth::ACTUAL));
    } else {
      for (const auto& src : node.srcs) {
        bitsPerElement = std::max(bitsPerElement, src->getBitsPerElement(PimBitWidth::ACTUAL));
        mm = src->getBitsPerElement(PimBitWidth::ACTUAL) * src->getMaxElementsPerRegion();
      }
      for (const auto& dst : node.dests) {
        bitsPerElement = std::max(bitsPerElement, dst->getBitsPerElement(PimBitWidth::ACTUAL));
        mm = dst->getBitsPerElement(PimBitWidth::ACTUAL) * dst->getMaxElementsPerRegion();
      }
    }
  }

  uint64_t maxBitsPerObj = has2Src1Dest ? totalGRFbits/2 : totalGRFbits;
  maxBitsPerObj = std::min<uint64_t>(maxBitsPerObj, static_cast<uint64_t>(mm));
  if (maxBitsPerObj < bitsPerElement) {
    printf("PIM-ERROR: Register is too small to hold the maximum bits per element.\n");
    return perfEnergies; // Return empty vector if error
  }
  unsigned numRegPerObj = maxBitsPerObj / m_blimpRegisterBitWidth;
  printf("PIM-Info: Fusing %zu commands.\n", cmdGraph.size());
  printf("PIM-Info: Has2Src1Dest %d Total GRF bits: %lu, Max bits per object: %lu, Num registers per object: %u\n",
         has2Src1Dest, totalGRFbits, maxBitsPerObj, numRegPerObj);

  for (size_t cmdId = 0; cmdId < cmdGraph.size(); ++cmdId) {
    // printf("PIM-Info: Processing command ID %zu, Type: %s, Available Registers: %d\n", 
    //        cmdId, pimCmd::getName(cmdGraph[cmdId].cmdType, "").c_str(), m_blimpRegisterCount - usedReg);
    // printf("LRU State: ");
    // for (const auto& regId : regLRU) {
    //   printf("%d ", regId);
    // }
    // printf("\n\n");
    auto& node = cmdGraph[cmdId];
    std::unordered_set<PimObjId> pinnedObjs;

    if (node.cmdType == PimCmdEnum::COPY_H2D) {
      PimObjId did = node.dests[0]->getObjId();
      if (inVectorRegister.count(did)) {
        inVectorRegister.erase(did);
        shouldWriteBack.erase(did);
        regLRU.remove(did);
        usedReg -= numRegPerObj;  // Decrease used register count
      } else if (inScalarRegister.count(did)) {
        inScalarRegister.erase(did);
        shouldWriteBack.erase(did);
      }
      continue;
    }

    if (node.cmdType == PimCmdEnum::COPY_D2H) {
      PimObjId sid = node.srcs[0]->getObjId();
      if (shouldWriteBack.count(sid)) {
        cmdGraph[shouldWriteBack[sid].first].numWrite += shouldWriteBack[sid].second;
        shouldWriteBack.erase(sid);
      }
      continue;
    }

    if (node.cmdType == PimCmdEnum::BROADCAST) {
      // This will be treated as writing to scalar register so no register management needed unless the broadcast value is being sent to host next
      PimObjId dstId  = node.dests[0]->getObjId();
      pinnedObjs.insert(dstId);
      pimObjInfo* dst  = node.dests[0];
      uint64_t bitsDst  = dst->getBitsPerElement(PimBitWidth::ACTUAL) * dst->getMaxElementsPerRegion();
      uint64_t numItr = std::ceil(static_cast<double>(bitsDst) / maxBitsPerObj);
      if (!inVectorRegister.count(dstId) && !inScalarRegister.count(dstId)) {
        // don't touch vector register as broadcast is scalar
        inScalarRegister[dstId] = true;
        if (broadcastShouldWriteBack(node)) shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
        // printf("PIM-WRITEBACK: Setting shouldWriteBack for dstId=%d with numItr=%lu in cmdID=%zu\n", 
        //        dstId, numItr, cmdId);
      } else {
        if (inVectorRegister.count(dstId)) {
          // remove from vector register if it exists there
          inVectorRegister.erase(dstId);
          inScalarRegister[dstId] = true;
          regLRU.remove(dstId);
          usedReg -= numRegPerObj;  // Decrease used register count
        }
        if (broadcastShouldWriteBack(node)) shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
      }
    } else if (node.srcs.size() == 2 && node.dests.size() == 1) {
    // Handle 2-src 1-dest pattern (e.g., Grfb = Bank ADD Grfa)
      // std::printf("Found 2-operand command: %s\n", pimCmd::getName(node.cmdType, "").c_str());
      PimObjId srcId1 = node.srcs[0]->getObjId();
      PimObjId srcId2 = node.srcs[1]->getObjId();
      PimObjId dstId  = node.dests[0]->getObjId();
      pinnedObjs.insert(srcId1);
      pinnedObjs.insert(srcId2);
      pinnedObjs.insert(dstId);

      pimObjInfo* src1 = node.srcs[0];
      // pimObjInfo* src2 = node.srcs[1];
      pimObjInfo* dst  = node.dests[0];

      uint64_t bitsSrc1 = src1->getBitsPerElement(PimBitWidth::ACTUAL) * src1->getMaxElementsPerRegion();
      // uint64_t bitsSrc2 = src2->getBitsPerElement(PimBitWidth::ACTUAL) * src2->getMaxElementsPerRegion();
      uint64_t bitsDst  = dst->getBitsPerElement(PimBitWidth::ACTUAL) * dst->getMaxElementsPerRegion();

      // Assume src1 and dst must fit in GRF
      uint64_t bitsNeededInGRF = bitsSrc1 + bitsDst;
      uint64_t numItr = std::ceil(static_cast<double>(bitsNeededInGRF) / totalGRFbits);

      // std::printf("Bits needed in GRF: %lu, Total GRF Bits: %lu\n", bitsNeededInGRF, totalGRFbits);

      // std::printf("   bits: src1=%lu, src2=%lu, dst=%lu | passes=%lu\n", bitsSrc1, bitsSrc2, bitsDst, numItr);

      // === src1 ===
      if (!inVectorRegister.count(srcId1) && !inScalarRegister.count(srcId1)) {
        node.numRead1 += numItr;
        if (!inVectorRegister.count(srcId2) && !inScalarRegister.count(srcId2)) {
          if (usedReg + numRegPerObj == m_blimpRegisterCount) {
            auto objID = regLRU.front();  // Evict the least recently used register
            if(pinnedObjs.count(objID)) {
              printf("PIM-Error: Cannot evict pinned object objID=%d for srcId1=%d in cmdID=%zu. Not enough registers available.\n", 
                     objID, srcId1, cmdId);
              return perfEnergies;  // Not enough registers available
            }
            regLRU.pop_front();
            inVectorRegister.erase(objID);
            // printf("PIM-EVICT: Evicting register objID=%d to make room for srcId=%d\n", objID, srcId1);
            usedReg -= numRegPerObj;  // Decrease used register count
            if (shouldWriteBack.count(objID)) {
              // printf("PIM-WRITEBACK: Incrementing numWrite by %lu for objID=%d in cmdID=%zu\n", 
              //       shouldWriteBack[objID].second, objID, shouldWriteBack[objID].first);
              if (isAliveAfter(cmdGraph[shouldWriteBack[objID].first], cmdId)) {
                cmdGraph[shouldWriteBack[objID].first].numWrite += shouldWriteBack[objID].second;
              }
              shouldWriteBack.erase(objID);
            }
          }
          usedReg += numRegPerObj;  // Increase used register count
          inVectorRegister[srcId1] = true;
          regLRU.push_back(srcId1);
        }
      } else {
        if (inVectorRegister.count(srcId1)) {
          regLRU.remove(srcId1);
          regLRU.push_back(srcId1);
        }
      }

      // === src2 (latched, but still requires row buffer access) ===
      if (!inVectorRegister.count(srcId2) && !inScalarRegister.count(srcId2)) {
        node.numRead2 += numItr;  // Still requires READ even if not stored in register
        // src2 is not stored in GRF → no inRegister/src2
      } else {
        if (inVectorRegister.count(srcId2)) {
          regLRU.remove(srcId2);
          regLRU.push_back(srcId2);
        }
      }

      // === dst ===
      if (!inVectorRegister.count(dstId)) {
        // printf("PIM-Info: dstId=%d not in register, used register=%d checking for eviction...\n", dstId, usedReg);
        if (inScalarRegister.count(dstId)) {
          // printf("PIM-Info: dstId=%d found in scalar register, promoting to vector register...\n", dstId);
          inScalarRegister.erase(dstId);
        }
        if (usedReg + numRegPerObj > m_blimpRegisterCount) {
          auto objID = regLRU.front();  // Evict the least recently used register
          if (pinnedObjs.count(objID)) {
            printf("PIM-Error: Cannot evict pinned object objID=%d for dstId=%d in cmdID=%zu. Not enough registers available.\n",
                   objID, dstId, cmdId);
            return perfEnergies;  // Not enough registers available
          }
          regLRU.pop_front();
          inVectorRegister.erase(objID);
          usedReg -= numRegPerObj;  // Decrease used register count
          // printf("PIM-EVICT: Evicting register objID=%d to make room for dstId=%d\n", objID, dstId);
          if (shouldWriteBack.count(objID)) {
            // printf("PIM-WRITEBACK: Incrementing numWrite by %lu for objID=%d in cmdID=%zu\n", 
            //        shouldWriteBack[objID].second, objID, shouldWriteBack[objID].first);
            if (isAliveAfter(cmdGraph[shouldWriteBack[objID].first], cmdId)) {
              cmdGraph[shouldWriteBack[objID].first].numWrite += shouldWriteBack[objID].second;
            }
            shouldWriteBack.erase(objID);
          }
        }
        usedReg += numRegPerObj;  // Increase used register count
        inVectorRegister[dstId] = true;
        shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
        regLRU.push_back(dstId);
      } else {
        regLRU.remove(dstId);
        regLRU.push_back(dstId);
        shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
      }
    } else if (node.srcs.size() == 1 && node.dests.size() == 1) {
      // Handle 1-src 1-dest pattern (e.g., Grf = Bank ADD Srf)
      // std::printf("Found 1-operand command: %s\n", pimCmd::getName(node.cmdType, "").c_str());

      PimObjId srcId = node.srcs[0]->getObjId();
      PimObjId dstId = node.dests[0]->getObjId();

      // pimObjInfo* src = node.srcs[0];
      pimObjInfo* dst = node.dests[0];

      // One can be latched, other one needs to be in GRF
      // uint64_t bitsSrc = src->getBitsPerElement(PimBitWidth::ACTUAL) * src->getMaxElementsPerRegion();
      uint64_t bitsDst = dst->getBitsPerElement(PimBitWidth::ACTUAL) * dst->getMaxElementsPerRegion();

      uint64_t numItr = std::ceil(static_cast<double>(bitsDst) / maxBitsPerObj);

      // std::printf("Bits needed in GRF: %lu, Total GRF Bits: %lu\n", bitsDst, maxBitsPerObj);
      // std::printf("   bits: src=%lu, dst=%lu | passes=%lu\n", bitsSrc, bitsDst, numItr);

      // === src ===
      if (!inVectorRegister.count(srcId) && !inScalarRegister.count(srcId)) {
        node.numRead1 += numItr;
      } else {
        if (inVectorRegister.count(srcId)) {
          regLRU.remove(srcId);
          regLRU.push_back(srcId);
        }
      }

      // === dst ===
      if (!inVectorRegister.count(dstId)) {
        if (inScalarRegister.count(dstId)) {
          // printf("PIM-Info: dstId=%d found in scalar register, promoting to vector register...\n", dstId);
          inScalarRegister.erase(dstId);
        }
        if (usedReg + numRegPerObj > m_blimpRegisterCount) {
          auto objID = regLRU.front();  // Evict the least recently used register
          if (pinnedObjs.count(objID)) {
            printf("PIM-Error: Cannot evict pinned object objID=%d for destId=%d in cmdID=%zu. Not enough registers available.\n", 
                    objID, dstId, cmdId);
            return perfEnergies;  // Not enough registers available
          }
          regLRU.pop_front();
          inVectorRegister.erase(objID);
          usedReg -= numRegPerObj;  // Decrease used register count
          // printf("PIM-EVICT: Evicting register objID=%d to make room for dstId=%d\n", objID, dstId);
          if (shouldWriteBack.count(objID)) {
            // printf("PIM-WRITEBACK: Incrementing numWrite by %lu for objID=%d in cmdID=%zu\n", 
            //        shouldWriteBack[objID].second, objID, shouldWriteBack[objID].first);

            if (isAliveAfter(cmdGraph[shouldWriteBack[objID].first], cmdId)) {
              cmdGraph[shouldWriteBack[objID].first].numWrite += shouldWriteBack[objID].second;
            }
            shouldWriteBack.erase(objID);
          }
        }
        usedReg += numRegPerObj;  // Increase used register count
        inVectorRegister[dstId] = true;
        shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
        regLRU.push_back(dstId);
      } else {
        regLRU.remove(dstId);
        regLRU.push_back(dstId);
        shouldWriteBack[dstId] = std::make_pair(cmdId, numItr);
      }
    } else if (node.srcs.size() == 2 && node.dests.empty()) {
      // Handle 1-src no-dest pattern (e.g., Grf = Bank READ)
      // std::printf("Found 1-src no-dest command: %s\n", pimCmd::getName(node.cmdType, "").c_str());
      PimObjId srcId1 = node.srcs[0]->getObjId();
      PimObjId srcId2 = node.srcs[1]->getObjId();
      pinnedObjs.insert(srcId1);
      pinnedObjs.insert(srcId2);

      pimObjInfo* src1 = node.srcs[0];
      // pimObjInfo* src2 = node.srcs[1];

      uint64_t bitsSrc1 = src1->getBitsPerElement(PimBitWidth::ACTUAL) * src1->getMaxElementsPerRegion();

      // Assume src1 and dst must fit in GRF
      uint64_t bitsNeededInGRF = bitsSrc1;
      uint64_t numItr = std::ceil(static_cast<double>(bitsNeededInGRF) / totalGRFbits);

      // std::printf("Bits needed in GRF: %lu, Total GRF Bits: %lu\n", bitsNeededInGRF, totalGRFbits);

      // std::printf("   bits: src1=%lu, src2=%lu, dst=%lu | passes=%lu\n", bitsSrc1, bitsSrc2, bitsDst, numItr);

      // === src1 ===
      if (!inVectorRegister.count(srcId1) && !inScalarRegister.count(srcId1)) {
        node.numRead1 += numItr;
        if (!inVectorRegister.count(srcId2) && !inScalarRegister.count(srcId2)) {
          if (usedReg + numRegPerObj == m_blimpRegisterCount) {
            auto objID = regLRU.front();  // Evict the least recently used register
            if(pinnedObjs.count(objID)) {
              printf("PIM-Error: Cannot evict pinned object objID=%d for srcId1=%d in cmdID=%zu. Not enough registers available.\n", 
                     objID, srcId1, cmdId);
              return perfEnergies;  // Not enough registers available
            }
            regLRU.pop_front();
            inVectorRegister.erase(objID);
            // printf("PIM-EVICT: Evicting register objID=%d to make room for srcId=%d\n", objID, srcId1);
            usedReg -= numRegPerObj;  // Decrease used register count
            if (shouldWriteBack.count(objID)) {
              // printf("PIM-WRITEBACK: Incrementing numWrite by %lu for objID=%d in cmdID=%zu\n", 
              //       shouldWriteBack[objID].second, objID, shouldWriteBack[objID].first);
              if (isAliveAfter(cmdGraph[shouldWriteBack[objID].first], cmdId)) {
                cmdGraph[shouldWriteBack[objID].first].numWrite += shouldWriteBack[objID].second;
              }
              shouldWriteBack.erase(objID);
            }
          }
          usedReg += numRegPerObj;  // Increase used register count
          inVectorRegister[srcId1] = true;
          regLRU.push_back(srcId1);
        }
      } else {
        if (inVectorRegister.count(srcId1)) {
          regLRU.remove(srcId1);
          regLRU.push_back(srcId1);
        }
      }

      // === src2 (latched, but still requires row buffer access) ===
      if (!inVectorRegister.count(srcId2) && !inScalarRegister.count(srcId2)) {
        node.numRead2 += numItr;  // Still requires READ even if not stored in register
        // src2 is not stored in GRF → no inRegister/src2
      } else {
        if (inVectorRegister.count(srcId1)) {
          regLRU.remove(srcId1);
          regLRU.push_back(srcId1);
        }
      }
    }
    // TODO: Add similar logic for 1-src 1-dest pattern if needed
  }

  // Flush any remaining registers that need write-back
  for (auto it = regLRU.begin(); it != regLRU.end();) {
    PimObjId id = *it;
    if (shouldWriteBack.count(id)) {
      bool shouldWriteBackFlag = false;
      for (auto& c : cmdGraph[shouldWriteBack[id].first].consumers) {
        if (cmdGraph[c].cmdType == PimCmdEnum::COPY_D2H) {
          cmdGraph[shouldWriteBack[id].first].numWrite += shouldWriteBack[id].second; // Count write for this command
          shouldWriteBack.erase(id);
          shouldWriteBackFlag = true;
          break;
        }
      }
      if (!shouldWriteBackFlag) {
        shouldWriteBack.erase(id);
      }
    }
    inVectorRegister.erase(id);
    it = regLRU.erase(it);
  }

  //  if (m_debugCmds) {
  // for (const auto& node : cmdGraph) {
  //   printf("Cmd ID %zu (%s):\n", node.cmdId, pimCmd::getName(node.cmdType, "").c_str());
  //   printf("  Srcs: ");
  //   for (const auto& src : node.srcs) {
  //     printf("%d ", src->getObjId());
  //   }
  //   printf("\n");
  //   printf("  Dest: ");
  //   if (!node.dests.empty()) printf("%d", node.dests[0]->getObjId());
  //   printf("\n");

  //   printf("  Producers: ");
  //   for (const auto& pid : node.producers) {
  //     printf("%zu ", pid);
  //   }
  //   printf("\n");
  //   printf("  Consumers: ");
  //   for (const auto& cid : node.consumers) {
  //     printf("%zu ", cid);
  //   }
  //   printf("\n");
  //   printf("  Num Read Src1: %u, Num Read Src2: %u, Num Write: %u\n", node.numRead1, node.numRead2, node.numWrite);
  //   printf("\n\n");
  // }
  // }
  // printf("PIM-Info: Fusion Depth: %u\n", fusionDepth);
  printf("Starting simulation...\n");
  // for (const auto& node : cmdGraph) {
  //   printf("Cmd ID %zu (%s):\n", node.cmdId, pimCmd::getName(node.cmdType, "").c_str());
  //   printf("  Srcs: ");
  //   for (const auto& src : node.srcs) {
  //     printf("%d ", src->getObjId());
  //   }
  //   printf("\n");
  //   printf("  Dest: ");
  //   if (!node.dests.empty()) printf("%d", node.dests[0]->getObjId());
  //   printf("\n");

  //   printf("  Producers: ");
  //   for (const auto& pid : node.producers) {
  //     printf("%zu ", pid);
  //   }
  //   printf("\n");
  //   printf("  Consumers: ");
  //   for (const auto& cid : node.consumers) {
  //     printf("%zu ", cid);
  //   }
  //   printf("\n");
  //   printf("  Num Read Src1: %u, Num Read Src2: %u, Num Write: %u\n", node.numRead1, node.numRead2, node.numWrite);
  //   printf("\n\n");
  // }
  simulateExecution(cmdGraph, perfEnergies);

  return perfEnergies;
}
