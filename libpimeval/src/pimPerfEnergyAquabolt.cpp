// File: pimPerfEnergyAquabolt.cc
// PIMeval Simulator - Performance Energy Models
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "pimPerfEnergyAquabolt.h"
#include "pimCmd.h"
#include <cstdio>
#include <cmath>
#include <list>
#include <sstream>
#include <unordered_set>

//! @brief  Perf energy model of aquabolt PIM for func1
pimeval::perfEnergy
pimPerfEnergyAquabolt::getPerfEnergyForFunc1(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned numCores = obj.getNumCoreAvailable();
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned numberOfOperationPerElement = std::ceil(bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
  unsigned elementsPerCore = std::ceil(obj.getNumElements() * 1.0 / numCores);
  unsigned minElementPerRegion = elementsPerCore > maxElementsPerRegion ? elementsPerCore - (maxElementsPerRegion * (numPass - 1)) : elementsPerCore;
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  double aquaboltCoreCycle = m_tGDL;
  unsigned numMaxActPre = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / (m_grfWidth * m_grfCount));
  unsigned numMinActPre = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / (m_grfWidth * m_grfCount));
  uint64_t totalOp = 0;
  unsigned numBankPerChip = numCores / m_numChipsPerRank;
  double activateMS = (m_tACT + (minGDLItr * m_tGDL)) < m_tRAS * m_tCK ? ((m_tRAS * m_tCK) - (minGDLItr * m_tGDL)) : m_tACT; // Use tRAS if GDL is less than tRAS

  switch (cmdType)
  {
    // Refer to Aquabolt Paper (Table 2, Figure 5). OP Format: GRF = BANK +/* SRF
    // Aquabolt has 16 16-bit vector registers (GRF) per PIM core.
    // As a result, depending on the bitsPerElement and columns per bank row, same row may be opened multiple times -- this is calculated as numActPre.
    case PimCmdEnum::ADD_SCALAR:
    case PimCmdEnum::MUL_SCALAR:
    { 
      msRead = ((m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre) + ((activateMS + m_tPRE) * numMinActPre);
      msWrite = ((m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre) + ((activateMS + m_tPRE) * numMinActPre);
      msCompute = (minGDLItr * aquaboltCoreCycle * numberOfOperationPerElement) + ((maxGDLItr * aquaboltCoreCycle * numberOfOperationPerElement) * (numPass - 1));
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = ((m_eACT + m_ePRE) * numMaxActPre * 2 + (maxElementsPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement)) * numCores * (numPass - 1);
      mjEnergy += ((m_eACT + m_ePRE) * numMinActPre * 2 +  (minElementPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement)) * numCores;
      mjEnergy += (m_eR_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      break;
    }
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

//! @brief  Perf energy model of aquabolt PIM for func2
pimeval::perfEnergy
pimPerfEnergyAquabolt::getPerfEnergyForFunc2(PimCmdEnum cmdType, const pimObjInfo& obj, const pimObjInfo& objSrc2, const pimObjInfo& objDest) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned numCoresUsed = obj.getNumCoreAvailable();
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned elementsPerCore = std::ceil(obj.getNumElements() * 1.0 / numCoresUsed);
  unsigned minElementPerRegion = elementsPerCore > maxElementsPerRegion ? elementsPerCore - (maxElementsPerRegion * (numPass - 1)) : elementsPerCore;
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  double aquaboltCoreCycle = m_tGDL;
  uint64_t totalOp = 0;
  unsigned numBankPerChip = numCoresUsed / m_numChipsPerRank;
  unsigned numMaxActPre = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 * 2 / (m_grfWidth * m_grfCount));
  unsigned numMinActPre = std::ceil(minElementPerRegion * bitsPerElement * 1.0 * 2/ (m_grfWidth * m_grfCount));
  double activateMS = (m_tACT + (minGDLItr * m_tGDL)) < m_tRAS * m_tCK ? ((m_tRAS * m_tCK) - (minGDLItr * m_tGDL)) : m_tACT; // Use tRAS if GDL is less than tRAS
  switch (cmdType)
  {
    // Refer to Aquabolt Paper (Table 2, Figure 5). OP Format: GRF = BANK +/* GRF
    case PimCmdEnum::ADD:
    case PimCmdEnum::MUL:
    {
      unsigned numberOfOperationPerElement = std::ceil(bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
      msRead = (2 * (m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre) + (maxGDLItr * m_tGDL * (numPass - 1)) + (2 * numMinActPre * (activateMS + m_tPRE)) + (minGDLItr * m_tGDL);
      msWrite = ((m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre) + (maxGDLItr * m_tGDL * (numPass - 1)) + (minGDLItr * m_tGDL) + (numMinActPre * (activateMS + m_tPRE));
      msCompute = (maxGDLItr * numberOfOperationPerElement * aquaboltCoreCycle) * (numPass - 1);
      msCompute += (minGDLItr * numberOfOperationPerElement * aquaboltCoreCycle);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 3 * numMaxActPre) + ((maxElementsPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement))) * numCoresUsed * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 3 * numMinActPre) + ((minElementPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement))) * numCoresUsed;
      mjEnergy += (m_eR_L * maxGDLItr * 2 * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * 2 * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
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
      // OP Format: GRF = BANK * SRF; GRF = BANK + GRF 
      unsigned numberOfOperationPerElement = std::ceil(bitsPerElement * 1.0 / m_aquaboltFPUBitWidth) * 2; // multiplying by 2 as one addition and one multiplication is needed
      msRead = ((m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre * 2) + (numMinActPre * (activateMS + m_tPRE) * 2);
      msWrite = (m_tACT + m_tPRE) * (numPass - 1) * numMaxActPre + (maxGDLItr * m_tGDL * (numPass - 1)) + (minGDLItr * m_tGDL) + (numMinActPre * (activateMS + m_tPRE));
      msCompute = (maxGDLItr * aquaboltCoreCycle * numberOfOperationPerElement) * (numPass - 1);
      msCompute += (minGDLItr * aquaboltCoreCycle * numberOfOperationPerElement);
      msRuntime = msRead + msWrite + msCompute;
      mjEnergy = (((m_eACT + m_ePRE) * 3 * numMaxActPre) + ((maxElementsPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement))) * numCoresUsed * (numPass - 1);
      mjEnergy += (((m_eACT + m_ePRE) * 3 * numMinActPre) + ((minElementPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement))) * numCoresUsed;
      mjEnergy += (m_eR_L * maxGDLItr * 2 * (numPass-1) * numBankPerChip * m_numRanks + (m_eR_L * 2 * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += (m_eW_L * maxGDLItr * (numPass-1) * numBankPerChip * m_numRanks + (m_eW_L * minGDLItr * numBankPerChip * m_numRanks));
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements() * 2;
      break;
    }
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
      printf("PIM-Warning: Unsupported for Aquabolt: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}


//! @brief  Perf energy model of aquabolt PIM for reduction sum
pimeval::perfEnergy
pimPerfEnergyAquabolt::getPerfEnergyForReduction(PimCmdEnum cmdType, const pimObjInfo& obj, unsigned numPass) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned numCore = obj.getNumCoreAvailable();
  double cpuTDP = 200; // W; AMD EPYC 9124 16 core
  unsigned elementsPerCore = std::ceil(obj.getNumElements() * 1.0 / numCore);
  unsigned minElementPerRegion = elementsPerCore > maxElementsPerRegion ? elementsPerCore - (maxElementsPerRegion * (numPass - 1)) : elementsPerCore;
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned numberOfOperationPerElement = std::ceil(bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
  double aquaboltCoreCycle = m_tGDL;
  uint64_t totalOp = 0;

  switch (cmdType) {
    case PimCmdEnum::REDSUM:
    case PimCmdEnum::REDSUM_RANGE:
    {
      msRead = (m_tR * numPass) + m_tGDL * numPass;
      msCompute = (maxGDLItr * aquaboltCoreCycle * numberOfOperationPerElement) * (numPass - 1);
      msCompute += (m_tR + (minGDLItr * aquaboltCoreCycle * numberOfOperationPerElement));
      msRuntime = msRead + msWrite + msCompute;
      // Refer to fulcrum documentation
      mjEnergy = (m_eAP + ((m_eR_L * maxGDLItr) + (maxElementsPerRegion * m_aquaboltArithmeticEnergy * numberOfOperationPerElement))) * numPass * numCore;
      // reduction for all regions
      double aggregateMs = static_cast<double>(numCore) / (3200000 * 16);
      msRuntime += aggregateMs;
      mjEnergy += aggregateMs * cpuTDP;
      mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;
      totalOp = obj.getNumElements();
      break;
    }
    case PimCmdEnum::REDMIN:
    case PimCmdEnum::REDMIN_RANGE:
    case PimCmdEnum::REDMAX:
    case PimCmdEnum::REDMAX_RANGE:
    default:
      printf("PIM-Warning: Unsupported for Aquabolt: %s\n", pimCmd::getName(cmdType, "").c_str());
      break;
  }
  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aquabolt PIM for broadcast
pimeval::perfEnergy
pimPerfEnergyAquabolt::getPerfEnergyForBroadcast(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  unsigned numPass = obj.getMaxNumRegionsPerCore();
  unsigned bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
  unsigned maxElementsPerRegion = obj.getMaxElementsPerRegion();
  unsigned numCore = obj.getNumCoreAvailable();
  uint64_t totalOp = 0;

  unsigned elementsPerCore = std::ceil(obj.getNumElements() * 1.0 / numCore);
  unsigned minElementPerRegion = elementsPerCore > maxElementsPerRegion ? elementsPerCore - (maxElementsPerRegion * (numPass - 1)) : elementsPerCore;
  unsigned maxGDLItr = std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  unsigned minGDLItr = std::ceil(minElementPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
  msWrite = (m_tW + maxGDLItr * m_tGDL) * (numPass - 1);
  msWrite += (m_tW + minGDLItr * m_tGDL);
  msRuntime = msRead + msWrite + msCompute;
  mjEnergy = (m_eAP + m_eR_L * maxGDLItr) * (numPass - 1) * numCore;
  mjEnergy += (m_eAP + m_eR_L * minGDLItr) * numCore;
  mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * msRuntime;

  return pimeval::perfEnergy(msRuntime, mjEnergy, msRead, msWrite, msCompute, totalOp);
}

//! @brief  Perf energy model of aquabolt PIM for rotate
pimeval::perfEnergy
pimPerfEnergyAquabolt::getPerfEnergyForRotate(PimCmdEnum cmdType, const pimObjInfo& obj) const
{
  double msRuntime = 0.0;
  double mjEnergy = 0.0;
  double msRead = 0.0;
  double msWrite = 0.0;
  double msCompute = 0.0;
  uint64_t totalOp = 0;
  printf("PIM-Warning: Unsupported for Aquabolt: %s\n", pimCmd::getName(cmdType, "").c_str());

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

    std::unordered_set<uint64_t> sortedIDs;
    for (auto* ev : sortedEvents) {
        sortedIDs.insert(ev->eventID);
    }

      for (auto* ev : allEvents) {
          if (!sortedIDs.count(ev->eventID)) {
              printf("Cycle EventID: %lu Type: %s Producers: ", ev->eventID, toString(ev->type).c_str());
              for (auto* p : ev->producers) printf("%lu ", p->eventID);
              printf("| Consumers: ");
              for (auto* c : ev->consumers) printf("%lu ", c->eventID);
              printf("\n");
          }
      }
  }


  return sortedEvents;
}

// ---- Helpers ----
static std::string joinIDs(const std::vector<pimeval::EventNode*>& v) {
  std::string s;
  for (size_t i = 0; i < v.size(); ++i) {
    s += std::to_string(v[i]->eventID);
    if (i + 1 < v.size()) s += ",";
  }
  return s.empty() ? "None" : s;
}

static std::string oneLine(const pimeval::EventNode* ev,
                           const std::vector<pimeval::cmdNode>& cmdGraph) {
  std::ostringstream oss;
  if (ev->type == pimeval::EventType::READ_SRC2) {
    oss << "EventID:" << ev->eventID
      << " CmdID:" << ev->cmdID
      << " " << pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "")
      << " P:" << ev->passId << ".2 C:" << ev->chunkID
      << " Type:" << toString(ev->type)
      << " | Prod:[" << joinIDs(ev->producers) << "]"
      << " | Cons:[" << joinIDs(ev->consumers) << "]";
  } else if (ev->type == pimeval::EventType::READ_SRC1) {
    oss << "EventID:" << ev->eventID
      << " CmdID:" << ev->cmdID
      << " " << pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "")
      << " P:" << ev->passId << ".1 C:" << ev->chunkID
      << " Type:" << toString(ev->type)
      << " | Prod:[" << joinIDs(ev->producers) << "]"
      << " | Cons:[" << joinIDs(ev->consumers) << "]";
  } else {
    oss << "EventID:" << ev->eventID
        << " CmdID:" << ev->cmdID
        << " " << pimCmd::getName(cmdGraph[ev->cmdID].cmdType, "")
        << " P:" << ev->passId << " C:" << ev->chunkID
        << " Type:" << toString(ev->type)
        << " | Prod:[" << joinIDs(ev->producers) << "]"
        << " | Cons:[" << joinIDs(ev->consumers) << "]";
  }
  return oss.str();
}

static std::string fit(const std::string& s, int width) {
  if ((int)s.size() <= width) return s + std::string(width - s.size(), ' ');
  if (width <= 3) return std::string(width, '.');
  return s.substr(0, width - 3) + "...";
}

// ---- Two-column print ----
void printTwoColumns(const std::vector<pimeval::EventNode*>& left,
                     const std::vector<pimeval::EventNode*>& right,
                     const std::vector<pimeval::cmdNode>& cmdGraph,
                     int colWidth = 80) {
  printf("======== Memory (left) || Compute (right) ========\n");
  const size_t rows = std::max(left.size(), right.size());
  for (size_t i = 0; i < rows; ++i) {
    std::string L = (i < left.size())  ? oneLine(left[i],  cmdGraph) : "";
    std::string R = (i < right.size()) ? oneLine(right[i], cmdGraph) : "";
    printf("%s  |  %s\n", fit(L, colWidth).c_str(), fit(R, colWidth).c_str());
  }
  printf("--------------------------------------------------\n");
}

std::string formatEvent(const pimeval::EventNode* e) {
    std::stringstream ss;
    ss << "EventID: " << e->eventID
      << ", CmdID: " << e->cmdID
      << ", PassID: " << e->passId
      << ", ChunkID: " << e->chunkID
      << ", Type: " << toString(e->type);
    return ss.str();
  };

void flipFlop(pimeval::cmdNode& cmd) {
  if (cmd.srcs.size() != 2) return;
  
  // printf("\n*** FlipFlop Optimization Starting ***\n");
  
  // First, let's see the input events
  // printf("Input events before optimization:\n");
  // size_t totalEvents = 0;
  // for (const auto& [passId, passEvents] : cmd.events) {
  //   for (const auto& [chunkId, eventList] : passEvents) {
  //     totalEvents += eventList.size();
  //     printf("Pass %u, Chunk %u: %zu events\n", passId, chunkId, eventList.size());
  //   }
  // }
  // printf("Total input events: %zu\n\n", totalEvents);

  // cmd.events: unordered_map<passId, unordered_map<chunkId, vector<EventNode*>>>
  // We want deterministic ordering: sort passes and chunks
  std::map<uint32_t, std::map<uint32_t, std::vector<pimeval::EventNode*>>> ordered;
  for (auto& [passId, eventList] : cmd.events) {
    for (auto& [chunkId, events] : eventList) {
      ordered[passId][chunkId] = events;
    }
  }

  std::map<uint32_t, std::vector<pimeval::EventNode*>> src1Events, src2Events;
  auto byEventId = [](auto* a, auto* b){ return a->eventID < b->eventID; };
  std::set<pimeval::EventNode*> eventsToRemoveFromCmd; // Track events to remove from cmd.events

  for (const auto& [passId, byChunk] : ordered) {
    auto& out1 = src1Events[passId];
    auto& out2 = src2Events[passId];

    // Walk chunks in ascending order, classify within chunk, then append to pass lists
    for (const auto& [chunkId, evs] : byChunk) {
      // Sort events in this chunk by time/id
      std::vector<pimeval::EventNode*> v = evs;
      std::sort(v.begin(), v.end(), byEventId);

      // Count reads for attribution
      int r1Total = 0, r2Total = 0;
      for (auto* e : v) {
        if (e->type == pimeval::EventType::READ_SRC1) ++r1Total;
        else if (e->type == pimeval::EventType::READ_SRC2) ++r2Total;
      }

      // Attribute ACTIVATE_READs to the source of the next READ in this chunk
      int a1 = 0, a2 = 0;
      for (size_t i = 0; i < v.size(); ++i) {
        auto* e = v[i];
        if (e->type != pimeval::EventType::ACTIVATE_READ) continue;

        bool attributed = false;
        for (size_t j = i + 1; j < v.size(); ++j) {
          if (v[j]->type == pimeval::EventType::READ_SRC1 && a1 < r1Total) {
            out1.push_back(e); ++a1; attributed = true; break;
          }
          if (v[j]->type == pimeval::EventType::READ_SRC2 && a2 < r2Total) {
            out2.push_back(e); ++a2; attributed = true; break;
          }
        }
        if (!attributed) { // fallback distribution
          if (a1 < r1Total) { out1.push_back(e); ++a1; }
          else if (a2 < r2Total) { out2.push_back(e); ++a2; }
        }
      }

      // Append READs in order
      for (auto* e : v) {
        if (e->type == pimeval::EventType::READ_SRC1) out1.push_back(e);
        else if (e->type == pimeval::EventType::READ_SRC2) out2.push_back(e);
      }

      // Attribute PRECHARGE_READs to most-recent reader in this chunk
      int lastReadSrc = 0; // 1 or 2
      for (auto* e : v) {
        if (e->type == pimeval::EventType::READ_SRC1) lastReadSrc = 1;
        else if (e->type == pimeval::EventType::READ_SRC2) lastReadSrc = 2;
        else if (e->type == pimeval::EventType::PRECHARGE_READ) {
          for (auto *p : e->producers) {
            if (p->type == pimeval::EventType::READ_SRC1) {lastReadSrc = 1; break;}
            else if (p->type == pimeval::EventType::READ_SRC2) {lastReadSrc = 2; break;}
          }
          if (lastReadSrc == 1) out1.push_back(e);
          else if (lastReadSrc == 2) out2.push_back(e);
        }
      }
    }

    // unsigned act = 0;
    // for (size_t i = 0; i < out2.size(); ++i) {
    //   auto* e = out2[i];
    //   if (act % 2 == 0) {
    //     if (e->type == pimeval::EventType::PRECHARGE_READ) {
    //       if (i + 1 < out2.size() && out2[i + 1]->type == pimeval::EventType::ACTIVATE_READ) {
    //         auto* evToDelete = out2[i + 1];
    //         //assumes only two consumers
    //         auto* c1 = evToDelete->consumers[0];
    //         auto* c2 = evToDelete->consumers[1];
    //         if (c1->type == pimeval::EventType::READ_SRC2) {
    //           c1->producers.erase(std::remove(c1->producers.begin(), c1->producers.end(), evToDelete), c1->producers.end());
              
    //         } 
    //         // else if (c1->type == pimeval::EventType::READ_SRC2 && c2->type == pimeval::EventType::READ_SRC1) {
    //         //   out2.erase(out2.begin() + i + 1);
    //         //   out1.push_back(evToDelete);
    //         // }

    //       }
    //     }
    //   }
    // }
    // Ensure final per-pass lists are sorted by (eventID) since we appended chunk-wise
    // std::sort(out1.begin(), out1.end(), byEventId);
    // std::sort(out2.begin(), out2.end(), byEventId);
    // printf("Src1 Events (P:%u): %zu:\n", passId, out1.size());
    // for (const auto* e : out1) {
    //   printf("  %s\t", formatEvent(e).c_str());
    //   printf(" | Prod:[ %s]"
    //     " | Cons:[%s]\n", joinIDs(e->producers).c_str(), joinIDs(e->consumers).c_str());
    // }
    // printf("Src2 Events (P:%u): %zu:\n", passId, out2.size());
    // for (const auto* e : out2) {
    //   printf("  %s\t", formatEvent(e).c_str());
    //   printf(" | Prod:[ %s]"
    //     " | Cons:[%s]\n", joinIDs(e->producers).c_str(), joinIDs(e->consumers).c_str());
    // }

    unsigned pre1 = 0;
    std::vector<size_t> pre1ToKeep; // Indices of PRECHARGE_READs to keep
    for (size_t i = 0; i < out1.size(); ++i) {
      auto* e = out1[i];
      if (!e || e->type != pimeval::EventType::PRECHARGE_READ) continue;
      pre1++;
      if (pre1 % 2) {
        // printf("Skipping PRECHARGE_READ: %s\n", formatEvent(e).c_str());
        // printf("producers: ");
        // for (auto* p : e->producers) {
        //   if (p) printf("%s \n", formatEvent(p).c_str());
        // }
        // printf("\nconsumers: ");
        // for (auto* c : e->consumers) {
        //   if (c) printf("%s \n", formatEvent(c).c_str());
        // }
        // printf("\n\n");
        continue; // act only on odd PRECHARGE_READs
      }
      // printf("precharge to keep: %s\n", formatEvent(e).c_str());
      pre1ToKeep.push_back(i);
    }
    // return;

    for (size_t t = 0; t < pre1ToKeep.size(); ++t) {
      auto* e = out1[pre1ToKeep[t]];
      size_t i = pre1ToKeep[t];

    // If the next event is ACTIVATE_READ in the same pass, do the fold/rewire
    if (i + 1 < out1.size()) {
      auto* next = out1[i + 1];
      if (next && next->type == pimeval::EventType::ACTIVATE_READ && next->passId == e->passId) {
        // printf("Event to delete: %s\n", formatEvent(next).c_str());
        eventsToRemoveFromCmd.insert(next); // Mark ACTIVATE_READ for removal from cmd.events
        // printf("\nConsumers of the Activate event to Delete: ");
        std::vector<pimeval::EventNode*> newProducers;   // READ_SRC2 producers we’ll attach to e
        pimeval::EventNode* actPre = nullptr;            // remember an ACTIVATE_WRITE (anchor)

        for (auto* c : next->consumers) {
          if (!c) continue;

          if (c->type == pimeval::EventType::PRECHARGE_READ) {
            // printf("\n\nThis PRECHARGE_READ will be eliminated: %s ", formatEvent(c).c_str());
            eventsToRemoveFromCmd.insert(c); // Mark PRECHARGE_READ for removal from cmd.events
            // find an ACTIVATE_WRITE among this precharge's consumers for anchoring
            // printf("\nConsumers of this PRECHARGE_READ: ");
            for (auto* cc : c->consumers) {
              if (!cc) continue;
              // printf("%s\n", formatEvent(cc).c_str());
              if (!actPre && cc->type == pimeval::EventType::ACTIVATE_READ) actPre = cc;
            }

            // collect READ_SRC2 producers; rewire them to consume e instead of this precharge
            // printf("\nProducers of this PRECHARGE_READ: ");
            for (auto* pp : c->producers) {
              if (!pp) continue;
              if (pp->type == pimeval::EventType::READ_SRC1) {
                if (std::find(newProducers.begin(), newProducers.end(), pp) == newProducers.end())
                  newProducers.push_back(pp);

              // remove c from pp->consumers
              auto& pcons = pp->consumers;
              pcons.erase(std::remove(pcons.begin(), pcons.end(), c), pcons.end());

              // add e to pp->consumers (dedup)
              if (std::find(pcons.begin(), pcons.end(), e) == pcons.end())
                pcons.push_back(e);
              }
              // printf("%s \n", formatEvent(pp).c_str());
            }
            // printf("\n");
            // printf("deleting PRECHARGE_READ: %s\n", formatEvent(c).c_str());
          }

          if (c->type == pimeval::EventType::READ_SRC1) {
            // Prefetch: move READ_SRC1 under the current open row by sourcing it from e's READ_SRC1 producers
            // printf("\nFollowing event will be prefetched: %s ", formatEvent(c).c_str());
            c->producers.clear();
            // collect first, then mutate e->producers to avoid iterator invalidation
            std::vector<pimeval::EventNode*> to_move;
            to_move.reserve(e->producers.size());
            for (auto* rr : e->producers) {
              if (rr && rr->type == pimeval::EventType::READ_SRC1) to_move.push_back(rr);
            }

            for (auto* rr : to_move) {
              // add rr to c->producers (dedup)
              if (std::find(c->producers.begin(), c->producers.end(), rr) == c->producers.end())
                c->producers.push_back(rr);

              // remove rr from e->producers
              auto& eprod = e->producers;
              eprod.erase(std::remove(eprod.begin(), eprod.end(), rr), eprod.end());

              // rewire rr->consumers: drop e, add c
              auto& rrcons = rr->consumers;
              rrcons.erase(std::remove(rrcons.begin(), rrcons.end(), e), rrcons.end());
              if (std::find(rrcons.begin(), rrcons.end(), c) == rrcons.end())
                rrcons.push_back(c);
            }

            // printf("\nNew Producers of this READ_SRC1: ");
            // for (auto* pp : c->producers) if (pp) printf("%s ", formatEvent(pp).c_str());
            // printf("\n");
          }
        }
        // printf("\nProducers of the Activate event to Delete: ");
        std::vector<pimeval::EventNode*> eventsToDelete; // Track events to delete
      
      for (auto* p : next->producers) {
        if (!p) continue;
        // printf("%s ", formatEvent(p).c_str());

        // remove 'next' from p->consumers
        auto& pcons = p->consumers;
        pcons.erase(std::remove(pcons.begin(), pcons.end(), next), pcons.end());

        // attach p -> actPre (if available)
        if (actPre) {
          if (std::find(pcons.begin(), pcons.end(), actPre) == pcons.end())
            pcons.push_back(actPre);
          auto& aprod = actPre->producers;
          if (std::find(aprod.begin(), aprod.end(), p) == aprod.end())
            aprod.push_back(p);
        }
      }
      // printf("\n");

      // Find and mark events to delete
      eventsToDelete.push_back(next); // Add the ACTIVATE_READ to deletion list
      for (auto* c : next->consumers) {
        if (c && c->type == pimeval::EventType::PRECHARGE_READ) {
          eventsToDelete.push_back(c); // Add PRECHARGE_READ to deletion list
        }
      }

      // Also mark these events for removal from cmd.events
      for (auto* delEv : eventsToDelete) {
        eventsToRemoveFromCmd.insert(delEv);
      }

      // Clean up deleted events from all producer/consumer lists
      for (auto* delEv : eventsToDelete) {
        // Remove from all producer lists
        for (auto* p : delEv->producers) {
          if (p) {
            auto& pcons = p->consumers;
            pcons.erase(std::remove(pcons.begin(), pcons.end(), delEv), pcons.end());
          }
        }
        // Remove from all consumer lists  
        for (auto* c : delEv->consumers) {
          if (c) {
            auto& cprod = c->producers;
            cprod.erase(std::remove(cprod.begin(), cprod.end(), delEv), cprod.end());
          }
        }
      }

      // Also remove deleted events from out2 list to prevent them from being processed further
      // for (auto* delEv : eventsToDelete) {
      //   auto it = std::find(out2.begin(), out2.end(), delEv);
      //   if (it != out2.end()) {
      //     out2.erase(it);
      //     // Adjust the loop index since we removed an element
      //     if (it <= out2.begin() + i) --i;
      //   }
      // }

      // Ensure e keeps at least one upstream producer if we collected any
      if (!newProducers.empty()) {
        if (std::find(e->producers.begin(), e->producers.end(), newProducers[0]) == e->producers.end())
          e->producers.push_back(newProducers[0]);
      }
    }
  }

  // printf("Stall this PRECHARGE and prefetch next reads—eliminate one ACTIVATE and one PRECHARGE.\n");
  // printf("Current PRECHARGE event ID: %lu \n", e->eventID);

  // printf("Updated Producer events:\n");
  // for (auto* p : e->producers) if (p) printf("  %s\n", formatEvent(p).c_str());
  // printf("\n\nUpdated Consumer events:\n");

  // Generic rewiring through ACTIVATE_READ descendants
  // for (auto* c : e->consumers) {
  //   if (!c) continue;
  //   // printf("  %s\n", formatEvent(c).c_str());
  //   if (c->type != pimeval::EventType::ACTIVATE_READ) continue;

  //   for (auto* cc : c->consumers) {
  //     if (!cc) continue;

  //     if (cc->type == pimeval::EventType::READ_SRC1) {
  //       // Move producers from PRECHARGE to READ (dedup instead of blind overwrite)
  //       for (auto* ep : e->producers) {
  //         if (!ep) continue;
  //         if (std::find(cc->producers.begin(), cc->producers.end(), ep) == cc->producers.end())
  //           cc->producers.push_back(ep);
  //       }
  //       e->producers.clear();

  //     } else if (cc->type == pimeval::EventType::PRECHARGE_READ) {
  //       // Replace e->producers with cc->producers minus the intermediate ACTIVATE
  //       e->producers = cc->producers;
  //       auto& v = e->producers;
  //       v.erase(std::remove(v.begin(), v.end(), c), v.end());
  //     }
  //   }
  // }
  // printf("\n");

  // printf("Updated PRECHARGE event ID: %lu \n", e->eventID);
  // printf("Updated producer events:\n");
  // for (auto* p : e->producers) if (p) printf("  %s\n", formatEvent(p).c_str());
  // printf("\n");
  }

  // hello hello hello


    // Starting operand 2 // src2
    unsigned pre = 0;
    std::vector<size_t> preToKeep; // Indices of PRECHARGE_READs to keep
    for (size_t i = 0; i < out2.size(); ++i) {
      auto* e = out2[i];
      if (!e || e->type != pimeval::EventType::PRECHARGE_READ) continue;
      pre++;
      if ((pre % 2) == 0) continue; // act only on odd PRECHARGE_READs
      // printf("precharge to keep: %s\n", formatEvent(e).c_str());
      preToKeep.push_back(i);
    }
    for (size_t t = 0; t < preToKeep.size(); ++t) {
      auto* e = out2[preToKeep[t]];
      size_t i = preToKeep[t];
      // if (!e || e->type != pimeval::EventType::PRECHARGE_READ) continue;

      // ++pre;
      // if ((pre % 2) == 0) continue; // act only on odd PRECHARGE_READs

    // If the next event is ACTIVATE_READ in the same pass, do the fold/rewire
    if (i + 1 < out2.size()) {
      auto* next = out2[i + 1];
      if (next && next->type == pimeval::EventType::ACTIVATE_READ && next->passId == e->passId) {
        // printf("Event to delete: %s\n", formatEvent(next).c_str());
        eventsToRemoveFromCmd.insert(next); // Mark ACTIVATE_READ for removal from cmd.events
        // printf("\nConsumers of the Activate event to Delete: ");
        std::vector<pimeval::EventNode*> newProducers;   // READ_SRC2 producers we’ll attach to e
        pimeval::EventNode* actPre = nullptr;            // remember an ACTIVATE_WRITE (anchor)

        for (auto* c : next->consumers) {
          if (!c) continue;

          if (c->type == pimeval::EventType::PRECHARGE_READ) {
            // printf("\n\nThis PRECHARGE_READ will be eliminated: %s ", formatEvent(c).c_str());
            eventsToRemoveFromCmd.insert(c); // Mark PRECHARGE_READ for removal from cmd.events
            // find an ACTIVATE_WRITE among this precharge's consumers for anchoring
            // printf("\nConsumers of this PRECHARGE_READ: ");
            for (auto* cc : c->consumers) {
              if (!cc) continue;
              // printf("%s\n", formatEvent(cc).c_str());
              if (!actPre && cc->type == pimeval::EventType::ACTIVATE_WRITE) actPre = cc;
            }

            // collect READ_SRC2 producers; rewire them to consume e instead of this precharge
            // printf("\nProducers of this PRECHARGE_READ: ");
            for (auto* pp : c->producers) {
              if (!pp) continue;
              if (pp->type == pimeval::EventType::READ_SRC2) {
                if (std::find(newProducers.begin(), newProducers.end(), pp) == newProducers.end())
                  newProducers.push_back(pp);

              // remove c from pp->consumers
              auto& pcons = pp->consumers;
              pcons.erase(std::remove(pcons.begin(), pcons.end(), c), pcons.end());

              // add e to pp->consumers (dedup)
              if (std::find(pcons.begin(), pcons.end(), e) == pcons.end())
                pcons.push_back(e);
              }
              // printf("%s \n", formatEvent(pp).c_str());
            }
            // printf("\n");
            // printf("deleting PRECHARGE_READ: %s\n", formatEvent(c).c_str());
          }

          if (c->type == pimeval::EventType::READ_SRC2) {
            // Prefetch: move READ_SRC2 under the current open row by sourcing it from e's READ_SRC2 producers
            // printf("\nFollowing event will be prefetched: %s ", formatEvent(c).c_str());
            c->producers.clear();
            // collect first, then mutate e->producers to avoid iterator invalidation
            std::vector<pimeval::EventNode*> to_move;
            to_move.reserve(e->producers.size());
            for (auto* rr : e->producers) {
              if (rr && rr->type == pimeval::EventType::READ_SRC2) to_move.push_back(rr);
            }

            for (auto* rr : to_move) {
              // add rr to c->producers (dedup)
              if (std::find(c->producers.begin(), c->producers.end(), rr) == c->producers.end())
                c->producers.push_back(rr);

              // remove rr from e->producers
              auto& eprod = e->producers;
              eprod.erase(std::remove(eprod.begin(), eprod.end(), rr), eprod.end());

              // rewire rr->consumers: drop e, add c
              auto& rrcons = rr->consumers;
              rrcons.erase(std::remove(rrcons.begin(), rrcons.end(), e), rrcons.end());
              if (std::find(rrcons.begin(), rrcons.end(), c) == rrcons.end())
                rrcons.push_back(c);
            }

            // printf("\nNew Producers of this READ_SRC2: ");
            // for (auto* pp : c->producers) if (pp) printf("%s ", formatEvent(pp).c_str());
            // printf("\n");
          }
        }
        // printf("\nProducers of the Activate event to Delete: ");
        std::vector<pimeval::EventNode*> eventsToDelete; // Track events to delete
      
      for (auto* p : next->producers) {
        if (!p) continue;
        // printf("%s ", formatEvent(p).c_str());

        // remove 'next' from p->consumers
        auto& pcons = p->consumers;
        pcons.erase(std::remove(pcons.begin(), pcons.end(), next), pcons.end());

        // attach p -> actPre (if available)
        if (actPre) {
          if (std::find(pcons.begin(), pcons.end(), actPre) == pcons.end())
            pcons.push_back(actPre);
          auto& aprod = actPre->producers;
          if (std::find(aprod.begin(), aprod.end(), p) == aprod.end())
            aprod.push_back(p);
        }
      }
      // printf("\n");

      // Find and mark events to delete
      eventsToDelete.push_back(next); // Add the ACTIVATE_READ to deletion list
      for (auto* c : next->consumers) {
        if (c && c->type == pimeval::EventType::PRECHARGE_READ) {
          eventsToDelete.push_back(c); // Add PRECHARGE_READ to deletion list
        }
      }

      // Also mark these events for removal from cmd.events
      for (auto* delEv : eventsToDelete) {
        eventsToRemoveFromCmd.insert(delEv);
      }

      // Clean up deleted events from all producer/consumer lists
      for (auto* delEv : eventsToDelete) {
        // Remove from all producer lists
        for (auto* p : delEv->producers) {
          if (p) {
            auto& pcons = p->consumers;
            pcons.erase(std::remove(pcons.begin(), pcons.end(), delEv), pcons.end());
          }
        }
        // Remove from all consumer lists  
        for (auto* c : delEv->consumers) {
          if (c) {
            auto& cprod = c->producers;
            cprod.erase(std::remove(cprod.begin(), cprod.end(), delEv), cprod.end());
          }
        }
      }

      // Also remove deleted events from out2 list to prevent them from being processed further
      // for (auto* delEv : eventsToDelete) {
      //   auto it = std::find(out2.begin(), out2.end(), delEv);
      //   if (it != out2.end()) {
      //     out2.erase(it);
      //     // Adjust the loop index since we removed an element
      //     if (it <= out2.begin() + i) --i;
      //   }
      // }

      // Ensure e keeps at least one upstream producer if we collected any
      if (!newProducers.empty()) {
        if (std::find(e->producers.begin(), e->producers.end(), newProducers[0]) == e->producers.end())
          e->producers.push_back(newProducers[0]);
      }
    }
  }

  // printf("Stall this PRECHARGE and prefetch next reads—eliminate one ACTIVATE and one PRECHARGE.\n");
  // printf("Current PRECHARGE event ID: %lu \n", e->eventID);

  // printf("Updated Producer events:\n");
  // for (auto* p : e->producers) if (p) printf("  %s\n", formatEvent(p).c_str());
  // printf("\n\nUpdated Consumer events:\n");

  // Generic rewiring through ACTIVATE_READ descendants
  for (auto* c : e->consumers) {
    if (!c) continue;
    // printf("  %s\n", formatEvent(c).c_str());
    if (c->type != pimeval::EventType::ACTIVATE_READ) continue;

    for (auto* cc : c->consumers) {
      if (!cc) continue;

      if (cc->type == pimeval::EventType::READ_SRC2) {
        // Move producers from PRECHARGE to READ (dedup instead of blind overwrite)
        for (auto* ep : e->producers) {
          if (!ep) continue;
          if (std::find(cc->producers.begin(), cc->producers.end(), ep) == cc->producers.end())
            cc->producers.push_back(ep);
        }
        e->producers.clear();

      } else if (cc->type == pimeval::EventType::PRECHARGE_READ) {
        // Replace e->producers with cc->producers minus the intermediate ACTIVATE
        e->producers = cc->producers;
        auto& v = e->producers;
        v.erase(std::remove(v.begin(), v.end(), c), v.end());
      }
    }
  }
  // printf("\n");

  // printf("Updated PRECHARGE event ID: %lu \n", e->eventID);
  // printf("Updated producer events:\n");
  // for (auto* p : e->producers) if (p) printf("  %s\n", formatEvent(p).c_str());
  // printf("\n");
  }

  // printf("DEBUG: About to perform final cleanup. Events to remove: %zu\n", eventsToRemoveFromCmd.size());

  // // Debug: Print summary of events to be removed
  // printf("\n=== FlipFlop Cleanup Summary ===\n");
  // printf("Events marked for removal: %zu\n", eventsToRemoveFromCmd.size());
  // for (auto* delEv : eventsToRemoveFromCmd) {
  //   printf("  Removing: %s\n", formatEvent(delEv).c_str());
  // }

  // Final cleanup: remove all deleted events from cmd.events nested structure
  for (auto* delEv : eventsToRemoveFromCmd) {
    // Search through the nested structure to find and remove the event
    for (auto& [passId, passEvents] : cmd.events) {
      for (auto& [chunkId, eventList] : passEvents) {
        eventList.erase(std::remove(eventList.begin(), eventList.end(), delEv), eventList.end());
      }
    }
  }

  // Debug: Print event list after deletion
  // printf("\n=== Event List After FlipFlop Cleanup ===\n");
  // for (const auto& [passId, passEvents] : cmd.events) {
  //   for (const auto& [chunkId, eventList] : passEvents) {
  //     printf("Pass %u, Chunk %u: %zu events\n", passId, chunkId, eventList.size());
  //     for (const auto* event : eventList) {
  //       printf("  %s\n", formatEvent(event).c_str());
  //     }
  //   }
  // }
  // printf("=== End Event List ===\n\n");
}
}



void
pimPerfEnergyAquabolt::simulateExecution(std::vector<pimeval::cmdNode>& cmdGraph, std::vector<pimeval::perfEnergy> &perfEnergies, int maxBitsPerObject) const {
  std::unordered_map<size_t, std::vector<pimeval::EventNode*>> cmdMap;
  unsigned numBanksPerChip = 1;
  unsigned numCores = 1;
  printf("maxBitsPerObject: %d, GDL Width: %d\n", maxBitsPerObject, m_GDLWidth);

  auto formatEvent = [&](pimeval::EventNode* e) -> std::string {
    std::stringstream ss;
    ss << "EventID: " << e->eventID
      << ", CmdID: " << e->cmdID
      << ", Cmd Type: " << pimCmd::getName(cmdGraph[e->cmdID].cmdType, "")
      << ", RowID: " << e->passId
      << ", ColumnID: " << e->chunkID
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
        if (c->type == pimeval::EventType::PRECHARGE_READ) {
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
        if (c->type == pimeval::EventType::PRECHARGE_WRITE) {
          c->earliestCycle = currCycle + m_tRAS;
        }
      }
      break;
    }
    case pimeval::EventType::PRECHARGE_READ:
    {
      ev->energyConsumed = m_ePRE * numCores;
      ev->cycleCount = m_tRP + ev->stalledCycle;
      cycleRequired = m_tRP + ev->stalledCycle;
      // printf("PIM-Info: Precharge read event %s has stalled for %d cycles\n", formatEvent(ev).c_str(), ev->stalledCycle);
      break;
    }
    case pimeval::EventType::PRECHARGE_WRITE:
    {
      ev->energyConsumed = m_ePRE * numCores;
      ev->cycleCount = m_tRP + ev->stalledCycle;
      cycleRequired = m_tRP + ev->stalledCycle;
      // printf("PIM-Info: Precharge write event %s has stalled for %d cycles\n", formatEvent(ev).c_str(), ev->stalledCycle);
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
    case PimCmdEnum::ADD_SCALAR:
    case PimCmdEnum::MUL_SCALAR:
    case PimCmdEnum::ADD:
    case PimCmdEnum::MUL:
    case PimCmdEnum::BROADCAST:
    {
      double itr = (ev->bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
      cycleRequired = std::ceil(m_tCCD_L * itr);
      ev->cycleCount = std::ceil(m_tCCD_L * itr);
      ev->energyConsumed = m_aquaboltArithmeticEnergy * numCores * itr;
      break;
    }
    case PimCmdEnum::SCALED_ADD:
    {
      double itr = (ev->bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
      cycleRequired = std::ceil(m_tCCD_L * 2 * itr);
      ev->cycleCount = std::ceil(m_tCCD_L * 2 * itr);
      ev->energyConsumed = m_aquaboltArithmeticEnergy * numCores * 2 * itr;
      break;
    }
    case PimCmdEnum::SUB_SCALAR:
    case PimCmdEnum::DIV_SCALAR:
    case PimCmdEnum::ABS:
    case PimCmdEnum::POPCOUNT:
    case PimCmdEnum::SUB:
    case PimCmdEnum::DIV:
    case PimCmdEnum::REDSUM:
    case PimCmdEnum::REDMIN:
    case PimCmdEnum::REDMAX:
    case PimCmdEnum::REDSUM_RANGE:
    case PimCmdEnum::REDMIN_RANGE:
    case PimCmdEnum::REDMAX_RANGE:
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
    case PimCmdEnum::COND_BROADCAST:
    case PimCmdEnum::COPY_O2O:
    case PimCmdEnum::BIT_SLICE_EXTRACT:
    case PimCmdEnum::BIT_SLICE_INSERT:
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
        // std::printf("[INFO] Sorted %zu events in PassID: %d, ChunkID: %d\n", cmdNode.events[passId][elem.first].size(), passId, elem.first);
        // for (auto* ev : cmdNode.events[passId][elem.first]) {
        //   printf("EventID: %lu EventType: %s \n", ev->eventID, toString(ev->type).c_str());
        // }
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
        if (!readActivatesByPass[0].empty() && !readPrechargesByPass.begin()->second.empty()) {
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
            precharge->producers.push_back(read1ByChunk[prechargeChunkId]);
            read1ByChunk[prechargeChunkId]->consumers.push_back(precharge);

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
            precharge->producers.push_back(read2ByChunk[prechargeChunkId]);
            read2ByChunk[prechargeChunkId]->consumers.push_back(precharge);

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

            precharge->producers.push_back(writeByChunk[prechargeChunkId]);
            writeByChunk[prechargeChunkId]->consumers.push_back(precharge);

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

  uint64_t currEventId = 0;
  for (auto& node : cmdGraph) {
    if (node.cmdType != PimCmdEnum::ADD && node.cmdType != PimCmdEnum::MUL &&
        node.cmdType != PimCmdEnum::SCALED_ADD && node.cmdType != PimCmdEnum::ADD_SCALAR &&
        node.cmdType != PimCmdEnum::MUL_SCALAR && node.cmdType != PimCmdEnum::BROADCAST) {
      continue; // Skip unsupported commands
    }

    pimObjInfo& obj = node.srcs.empty() ? *node.dests[0] : *node.srcs[0];
    uint64_t numPass = obj.getMaxNumRegionsPerCore();
    uint64_t bitsPerElement = obj.getBitsPerElement(PimBitWidth::ACTUAL);
    numCores = obj.isLoadBalanced() ? obj.getNumCoreAvailable() : obj.getNumCoresUsed();
    uint64_t maxElementsPerRegion = obj.getMaxElementsPerRegion();
    uint64_t totalElements = obj.getNumElements();
    uint64_t maxGdlItr = m_GDLWidth > maxBitsPerObject ? std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / maxBitsPerObject) : std::ceil(maxElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
    numBanksPerChip = numCores / m_numChipsPerRank;

    // Calculate minElementsPerRegion safely to avoid underflow
    uint64_t elementsPerCore = std::ceil(totalElements * 1.0 / numCores);
    uint64_t elementsInMaxPasses = maxElementsPerRegion * (numPass - 1);
    uint64_t minElementsPerRegion = (elementsPerCore > elementsInMaxPasses) ? 
                                    (elementsPerCore - elementsInMaxPasses) : 
                                    maxElementsPerRegion;
    uint64_t minGdlItr = m_GDLWidth > maxBitsPerObject ? std::ceil(minElementsPerRegion * bitsPerElement * 1.0 / maxBitsPerObject) : std::ceil(minElementsPerRegion * bitsPerElement * 1.0 / m_GDLWidth);
    unsigned R1 = node.numRead1 == 0 ? 1 : node.numRead1, R2 = node.numRead2 == 0 ? 1 : node.numRead2;
    unsigned W = node.numWrite == 0 ? 1 : node.numWrite;
    unsigned W_stride = std::ceil(maxGdlItr / W), R1_stride = std::ceil(maxGdlItr / R1), R2_stride = std::ceil(maxGdlItr / R2);
    // std::printf("Read1: %u, R1_Stride: %u, Read2: %u, R2_Stride: %u, Write: %u, W_Stride: %u\n", R1, R1_stride, R2, R2_stride, W, W_stride);
    // printf("PIM-Info: Command ID: %zu Type: %s NumPass: %lu MaxGdlItr: %lu MinGdlItr: %lu MaxElement: %lu MinElement: %lu\n", 
    //        node.cmdId, pimCmd::getName(node.cmdType, "").c_str(), numPass, maxGdlItr, minGdlItr, maxElementsPerRegion, minElementsPerRegion);
    if (node.cmdType == PimCmdEnum::BROADCAST || node.cmdType == PimCmdEnum::COND_BROADCAST) {
      if (node.numWrite == 0) {
        // If no write, we only need to read once
        perfEnergies[node.cmdId].m_msCompute = m_tCCD_L * m_tCK;
        perfEnergies[node.cmdId].m_mjEnergy = m_aquaboltArithmeticEnergy * numCores * (bitsPerElement * 1.0 / m_aquaboltFPUBitWidth);
        continue;
      }
      // std::printf("Broadcast command detected: %s Command ID: %zu Has Write: %d\n", pimCmd::getName(node.cmdType, "").c_str(), node.cmdId, node.numWrite);
      for (uint64_t p = 0; p < numPass; ++p) {
        uint64_t totalChunks = p < numPass - 1 ? maxGdlItr : minGdlItr;
        totalChunks = std::max(totalChunks, static_cast<uint64_t>(node.numWrite > 0 ? W : 1));
        unsigned writeACh = 0, writePCh = W_stride - 1;
        for (uint64_t c = 0; c < totalChunks; ++c) {
          if (node.numWrite > 0) {
            if (c == writeACh) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
              writeACh += W_stride;
            }
            if (c == writePCh || c == totalChunks - 1) {
              pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_WRITE, currEventId++, node.cmdId, c, p, bitsPerElement);
              cmdMap[node.cmdId].push_back(en);
              node.events[p][c].push_back(en);
              writePCh += W_stride;
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
        uint64_t totalChunks = p < numPass - 1 ? maxGdlItr : minGdlItr;
        // uint64_t tempCh = std::max(node.numRead1, node.numRead2);
        // tempCh = std::max(tempCh, static_cast<uint64_t>(node.numWrite));
        // totalChunks = std::max(totalChunks, tempCh);
        // printf("PIM-Info: Pass %lu, Total Chunks: %lu\n", p, totalChunks);
        unsigned writeACh = 0, writePCh = W_stride - 1, read1Ch = 0, read1PCh = R1_stride - 1, read2Ch = 0, read2PCh = R2_stride -1 ;
        // if (p == 0 && (node.cmdType == PimCmdEnum::ADD_SCALAR || node.cmdType == PimCmdEnum::SUB_SCALAR ||
        //     node.cmdType == PimCmdEnum::MUL_SCALAR || node.cmdType == PimCmdEnum::DIV_SCALAR ||
        //     node.cmdType == PimCmdEnum::MIN_SCALAR || node.cmdType == PimCmdEnum::MAX_SCALAR ||
        //     node.cmdType == PimCmdEnum::GT_SCALAR || node.cmdType == PimCmdEnum::LT_SCALAR ||
        //     node.cmdType == PimCmdEnum::EQ_SCALAR || node.cmdType == PimCmdEnum::NE_SCALAR ||
        //     node.cmdType == PimCmdEnum::AND_SCALAR || node.cmdType == PimCmdEnum::OR_SCALAR ||
        //     node.cmdType == PimCmdEnum::XOR_SCALAR || node.cmdType == PimCmdEnum::XNOR_SCALAR || 
        //     node.cmdType == PimCmdEnum::SCALED_ADD)) {
        //     pimeval::EventNode* en = pimeval::generateEvent(pimeval::EventType::ACTIVATE_READ, currEventId++, node.cmdId, 0, p, bitsPerElement);
        //     cmdMap[node.cmdId].push_back(en);
        //     node.events[p][0].push_back(en);
        //     en = pimeval::generateEvent(pimeval::EventType::READ_SCALAR, currEventId++, node.cmdId, 0, p, bitsPerElement);
        //     cmdMap[node.cmdId].push_back(en);
        //     node.events[p][0].push_back(en);
        //     en = pimeval::generateEvent(pimeval::EventType::PRECHARGE_READ, currEventId++, node.cmdId, 0, p, bitsPerElement);
        //     cmdMap[node.cmdId].push_back(en);
        //     node.events[p][0].push_back(en);
        // }
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
    if (node.canPrefetch) {
      // printf("[INFO] Prefetch optimization applied for CmdID: %zu, CmdType: %s\n", node.cmdId, pimCmd::getName(node.cmdType, "").c_str());
      flipFlop(node);
    }
    // printf("Intra-CMD edges created for CmdID: %zu, CmdType: %s\n", node.cmdId, pimCmd::getName(node.cmdType, "").c_str());
    // for (const auto& [passId, chunkMap] : node.events) {
    //   for (const auto& [chunkId, eventList] : chunkMap) {
    //     for (const auto* ev : eventList) {
    //       printf("ID: %lu Type: %s\n", ev->eventID, toString(ev->type).c_str());
    //       printf("Producers: ");
    //       for (const auto* p : ev->producers) {
    //         printf("ID: %lu\t", p->eventID);
    //       }
    //       printf("\n");
    //       printf("Consumers: ");
    //       for (const auto* c : ev->consumers) {
    //         printf("ID: %lu\t", c->eventID);
    //       }
    //       printf("\n");
    //     }
    //     printf("\n");
    //   }
    // }
  }
  // return;
  createInterCMDEdge();
  
  // std::vector<pimeval::EventNode*> sortedEvents = createSortedEventList(cmdGraph, false, false);
  // printf("========= Sorted Event List =========\n");
  // for (const auto* ev : sortedEvents) {
  //   printf("EventID: %lu, CmdID: %zu, Cmd Type: %s, RowID: %u, ColumnID: %u, Type: %s\n    Producers: ",
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
  // printTwoColumns(memoryEvents, computeEvents, cmdGraph, /*colWidth=*/90);

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
      }
      if (compCycles > 0) {
        hasCompute = true;
        compReady = clockCycle + compCycles;
        compEv->hasExecuted = true; // Mark as executed
        computeEvents.erase(computeEvents.begin());
        perfEnergies[compEv->cmdID].m_msCompute += compCycles * m_tCK;
        perfEnergies[compEv->cmdID].m_mjEnergy += compEv->energyConsumed;
        if (lastMemEv.first != nullptr && lastMemEv.second > 0) {
          lastMemEv.second -= compCycles * m_tCK; // Adjust last memory event duration
          lastMemEv.second = std::max(lastMemEv.second, 0.0); // Ensure non-negative
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
    if (c.cmdType == PimCmdEnum::MUL_SCALAR || c.cmdType == PimCmdEnum::ADD_SCALAR) {
      perfEnergies[c.cmdId].m_msRead += m_tCCD_L * m_tCK;
    }
    perfEnergies[c.cmdId].m_msRuntime = perfEnergies[c.cmdId].m_msRead + perfEnergies[c.cmdId].m_msWrite + perfEnergies[c.cmdId].m_msCompute;
    perfEnergies[c.cmdId].m_mjEnergy += m_pBChip * m_numChipsPerRank * m_numRanks * perfEnergies[c.cmdId].m_msRuntime;
  }
}

std::vector<pimeval::perfEnergy>
pimPerfEnergyAquabolt::getPerfEnergyForPIMProg(std::vector<pimeval::cmdNode>& cmdGraph) const {
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
  
  std::vector<pimeval::perfEnergy> perfEnergies(cmdGraph.size(), pimeval::perfEnergy(0, 0, 0, 0, 0, 0));
  std::unordered_map<PimObjId, bool> inVectorRegister;
  std::unordered_map<PimObjId, bool> inScalarRegister;
  std::unordered_map<PimObjId, std::pair<size_t, uint64_t>> shouldWriteBack;
  std::list<PimObjId> regLRU;
  unsigned usedReg = 0;

  uint64_t totalGRFbits = m_grfCount * m_grfWidth;
  // max allowable register bits per object should be what each operand of a 2 src 1 dest command can hold
  // assuming one of the srcs is latched 
  bool has2Src1Dest = false;
  unsigned mm = 0;
  unsigned bitsPerElement = 0;

  for (const auto& node : cmdGraph) {
    if (node.srcs.size() == 2 && node.dests.size() == 1 &&
        (node.cmdType == PimCmdEnum::ADD || node.cmdType == PimCmdEnum::MUL ||
         node.cmdType == PimCmdEnum::SCALED_ADD || node.cmdType == PimCmdEnum::ADD_SCALAR ||
         node.cmdType == PimCmdEnum::MUL_SCALAR)) {
      has2Src1Dest = true;
      mm = node.srcs[0]->getBitsPerElement(PimBitWidth::ACTUAL) * node.srcs[0]->getMaxElementsPerRegion();
      bitsPerElement = std::max(bitsPerElement, node.srcs[0]->getBitsPerElement(PimBitWidth::ACTUAL));
    } else if ((node.srcs.size() == 1 && node.dests.size() == 1) ||
               (node.cmdType == PimCmdEnum::BROADCAST)) {
      mm = node.dests[0]->getBitsPerElement(PimBitWidth::ACTUAL) * node.dests[0]->getMaxElementsPerRegion();
      bitsPerElement = std::max(bitsPerElement, node.dests[0]->getBitsPerElement(PimBitWidth::ACTUAL));
    }
  }

  uint64_t maxBitsPerObj = has2Src1Dest ? totalGRFbits/2 : totalGRFbits;
  maxBitsPerObj = std::min<uint64_t>(maxBitsPerObj, static_cast<uint64_t>(mm));
  if (maxBitsPerObj < bitsPerElement) {
    printf("PIM-ERROR: Register is too small to hold the maximum bits per element.\n");
    return perfEnergies; // Return empty vector if error
  }
  unsigned numRegPerObj = maxBitsPerObj / m_grfWidth;
  printf("PIM-Info: Fusing %zu commands.\n", cmdGraph.size());
  // printf("PIM-Info: Has2Src1Dest %d Total GRF bits: %lu, Max bits per object: %lu, Num registers per object: %u\n",
        //  has2Src1Dest, totalGRFbits, maxBitsPerObj, numRegPerObj);

  for (size_t cmdId = 0; cmdId < cmdGraph.size(); ++cmdId) {
    // printf("PIM-Info: Processing command ID %zu, Type: %s, Available Registers: %d\n", 
    //        cmdId, pimCmd::getName(cmdGraph[cmdId].cmdType, "").c_str(), m_grfCount - usedReg);
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

    if (node.cmdType != PimCmdEnum::ADD && node.cmdType != PimCmdEnum::MUL &&
        node.cmdType != PimCmdEnum::SCALED_ADD && node.cmdType != PimCmdEnum::ADD_SCALAR &&
        node.cmdType != PimCmdEnum::MUL_SCALAR && node.cmdType != PimCmdEnum::BROADCAST) {
      continue; // Skip unsupported commands
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
          if (usedReg + numRegPerObj == m_grfCount) {
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
        if (usedReg + numRegPerObj > m_grfCount) {
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
        if (usedReg + numRegPerObj > m_grfCount) {
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
          if (usedReg + numRegPerObj == m_grfCount) {
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
  unsigned fusionDepth = 0;
  for (auto& node: cmdGraph) {
    bool hasProdDep = false, hasConDep = false;
    if (node.cmdType != PimCmdEnum::ADD && node.cmdType != PimCmdEnum::MUL &&
        node.cmdType != PimCmdEnum::SCALED_ADD && node.cmdType != PimCmdEnum::ADD_SCALAR &&
        node.cmdType != PimCmdEnum::MUL_SCALAR && node.cmdType != PimCmdEnum::BROADCAST) {
      continue; // Skip unsupported commands
    }
    for (const auto& pid : node.producers) {
      if (cmdGraph[pid].cmdType != PimCmdEnum::COPY_H2D && cmdGraph[pid].cmdType != PimCmdEnum::COPY_D2H && cmdGraph[pid].cmdType != PimCmdEnum::COPY_D2D) {
        hasProdDep = true;
        break;
      }
    }
    for (const auto& cid : node.consumers) {
      if (cmdGraph[cid].cmdType != PimCmdEnum::COPY_H2D && cmdGraph[cid].cmdType != PimCmdEnum::COPY_D2H && cmdGraph[cid].cmdType != PimCmdEnum::COPY_D2D) {
        hasConDep = true;
        break;
      }
    }
    if (!hasProdDep && !hasConDep) {
      node.canPrefetch = true;
      fusionDepth = 0;
    } else {
      fusionDepth++;
    }
  }
  // printf("PIM-Info: Fusion Depth: %u\n", fusionDepth);
  printf("Starting simulation...\n");
  simulateExecution(cmdGraph, perfEnergies, maxBitsPerObj);
  return perfEnergies;
}