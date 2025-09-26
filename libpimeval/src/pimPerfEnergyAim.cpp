// File: pimPerfEnergyAim.cc
// PIMeval Simulator - Performance Energy Models
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "pimPerfEnergyAim.h"
#include "pimCmd.h"
#include <cmath>
#include <cstdio>

// AiM adds a SIMD Multiplier and a Reduction Tree in each bank.
// The supported instructions are: MAC.
// For simplicity, the SIMD lane width is assumed to be determined by the GDL width of the HBM/DDR memory.
// NOTE: The energy model is approximated. 

//! @brief  Perf energy model of aim PIM for func1
pimeval::perfEnergy
pimPerfEnergyAim::getPerfEnergyForFunc1(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;
  switch (cmdType)
  {
    // Refer to AiM Paper (Table 2, Figure 5). OP Format: GRF = BANK +/* SRF
    case PimCmdEnum::ADD_SCALAR:
    case PimCmdEnum::MUL_SCALAR:
    case PimCmdEnum::AES_SBOX:
    case PimCmdEnum::AES_INVERSE_SBOX:
    case PimCmdEnum::POPCOUNT:
    case PimCmdEnum::ABS:
    case PimCmdEnum::SUB_SCALAR:
    case PimCmdEnum::DIV_SCALAR:
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
    case PimCmdEnum::SHIFT_BITS_L:
    case PimCmdEnum::SHIFT_BITS_R:
    default:
      printf("PIM-Warning: Perf energy model not available for PIM command %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aim for func2
pimeval::perfEnergy
pimPerfEnergyAim::getPerfEnergyForFunc2(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objSrc2, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;
  switch (cmdType)
  {
    // Refer to Aquabolt Paper (Table 2, Figure 5). OP Format: GRF = BANK +/* GRF
    case PimCmdEnum::ADD:
    case PimCmdEnum::MUL:
    case PimCmdEnum::SCALED_ADD:
    case PimCmdEnum::DIV:
    case PimCmdEnum::SUB:
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
    default:
      printf("PIM-Warning: Unsupported for AiM: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aim PIM for reduction sum
pimeval::perfEnergy
pimPerfEnergyAim::getPerfEnergyForReduction(PimCmdEnum cmdType, const pimObjInfo& obj, unsigned numPass) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;

  switch (cmdType) {
    case PimCmdEnum::REDSUM:
    case PimCmdEnum::REDSUM_RANGE:
    case PimCmdEnum::REDMIN:
    case PimCmdEnum::REDMIN_RANGE:
    case PimCmdEnum::REDMAX:
    case PimCmdEnum::REDMAX_RANGE:
    default:
      printf("PIM-Warning: Unsupported for AiM: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aim for broadcast
pimeval::perfEnergy
pimPerfEnergyAim::getPerfEnergyForBroadcast(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aim for rotate
pimeval::perfEnergy
pimPerfEnergyAim::getPerfEnergyForRotate(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;
  printf("PIM-Warning: Unsupported for AiM: %s\n", pimCmd::getName(cmdType, "").c_str());

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

pimeval::perfEnergy pimPerfEnergyAim::getPerfEnergyForMac(PimCmdEnum cmdType, const pimObjInfo& objSrc, const pimObjInfo& objDest) const
{
  // NumPass is always 1 for MAC operation in AiM. User really needs to make sure that this holds true.
  // Buffer read time is `tCCD_S * tCK`.
  // User may wonder why buffer read time is not multiplied by number of banks per chip. This is because according the AiM paper, the buffer is n-way fanout to n banks in the same chip.
  // AiM paper mentions accumulation reduction tree requires 4 cycles. Hence, the compute time for accumulation is `4 * tCK`.
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;
  unsigned bitsPerElement = objSrc.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = objSrc.getMaxElementsPerRegion();
  unsigned numCore = objSrc.getNumCoreAvailable();
  unsigned maxGdlItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned numBankPerChip = numCore / m_numChipsPerRank;
  unsigned numPass = objDest.getMaxNumRegionsPerCore();
  uint64_t totalElements = objDest.getNumElements();
  uint64_t usedInPrevPasses = maxElementsPerRegion * (numPass - 1);
  uint64_t minElementsPerRegion = (totalElements > usedInPrevPasses) ? (totalElements - usedInPrevPasses) : totalElements;
  unsigned minGdlItr = std::ceil(minElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  pimeval::perfEnergy perfEnergyBT = getPerfEnergyForBytesTransfer(PimCmdEnum::COPY_D2H, (bitsPerElement * numCore) / 8);
  double bufferAccessLat = m_tCCD_S * m_tCK;
  msRead = ((m_tACT + m_tPRE) * numPass) + (bufferAccessLat * maxGdlItr * (numPass - 1)) + (bufferAccessLat * minGdlItr);
  msWrite = perfEnergyBT.m_msRuntime;
  msCompute = ((maxGdlItr * m_tGDL) * (numPass - 1)) + (minGdlItr * m_tGDL) + (4 * m_tCK * numPass);
  msRuntime = msRead + msWrite + msCompute;
  mjEnergy = ((m_eACT + m_ePRE) + (maxElementsPerRegion * m_aquaboltArithmeticEnergy * 2)) * numCore * (numPass - 1);
  mjEnergy += ((m_eACT + m_ePRE) + (minElementsPerRegion * m_aquaboltArithmeticEnergy * 2)) * numCore; // Energy for last pass
  mjEnergy += ((m_eR_L + m_eR_S) * numBankPerChip * m_numRanks * maxGdlItr * (numPass - 1)) + ((m_eR_L + m_eR_S) * numBankPerChip * m_numRanks * minGdlItr); // Energy for reading data from local row buffer to global row buffer
  mjEnergy += perfEnergyBT.m_mjEnergy;
  mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
  totalOp = objSrc.getNumElements() * 2;
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

// AiM does not have any register set--making it not being able to exploit any fusion opportunities.
//! @brief  Perf energy model of aim for PIM program
std::vector<pimeval::perfEnergy>
pimPerfEnergyAim::getPerfEnergyForPIMProg(std::vector<pimeval::cmdNode>& cmdGraph) const
{
  std::vector<pimeval::perfEnergy> perfEnergies(cmdGraph.size(), pimeval::perfEnergy(0.0, 0.0, 0.0, 0.0, 0.0, 0));
  for (const auto& cmdNode : cmdGraph)
  {
    switch (cmdNode.cmdType)
    {
    case PimCmdEnum::MAC:
      perfEnergies[cmdNode.cmdId] = getPerfEnergyForMac(cmdNode.cmdType, *cmdNode.srcs[0], *cmdNode.srcs[1]);
      break;
    case PimCmdEnum::COPY_H2D:
    case PimCmdEnum::COPY_D2H:
    case PimCmdEnum::COPY_D2D:
    case PimCmdEnum::COPY_O2O:
      break;
    default:
      printf("PIM-Warning: Unsupported PIM command for AiM: %s\n", pimCmd::getName(cmdNode.cmdType, "").c_str());
      break;
    }
  }
  return perfEnergies;
}