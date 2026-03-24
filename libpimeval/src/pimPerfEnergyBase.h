// File: pimPerfEnergyBase.h
// PIMeval Simulator - Performance Energy Models
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef LAVA_PIM_PERF_ENERGY_BASE_H
#define LAVA_PIM_PERF_ENERGY_BASE_H

#include "libpimeval.h"                // for PimDeviceEnum, PimDataType
#include "pimParamsDram.h"             // for pimParamsDram
#include "pimCmd.h"                    // for PimCmdEnum
#include "pimResMgr.h"                 // for pimObjInfo
#include <cstdint>
#include <cstdio>
#include <memory>                      // for std::unique_ptr


namespace pimeval {

  struct AquaboltConfig {
    unsigned grfWidth = 256;
    unsigned grfCount = 16;
    unsigned srfCount = 16;
    unsigned srfBitWidth = 256;
  };

  struct BankLevelConfig {
    unsigned blimpRegisterCount = 3;
    unsigned blimpRegisterBitWidth = 0; // 0 means use GDLWidth as default
    unsigned blimpScalarRegisterCount = 0; // 0 means unlimited (no capacity enforcement)
  };

  class perfEnergy
  {
    public:
      perfEnergy() : m_msRuntime(0.0), m_mjEnergy(0.0), m_msRead(0.0), m_msWrite(0.0), m_msCompute(0.0), m_totalOp(0), m_totalACT(0), m_totalPRE(0), m_totalCAS(0), m_totalL(0) {}
      perfEnergy(double msRuntime, double mjEnergy, double msRead, double msWrite, double msCompute, uint64_t totalOp, uint64_t totalACT = 0, uint64_t totalPRE = 0, uint64_t totalCAS = 0, uint64_t totalL = 0) : m_msRuntime(msRuntime), m_mjEnergy(mjEnergy), m_msRead(msRead), m_msWrite(msWrite), m_msCompute(msCompute), m_totalOp(totalOp), m_totalACT(totalACT), m_totalPRE(totalPRE), m_totalCAS(totalCAS), m_totalL(totalL) {}

      double m_msRuntime;
      double m_mjEnergy;
      double m_msRead;
      double m_msWrite;
      double m_msCompute;
      uint64_t m_totalOp;
      uint64_t m_totalACT;
      uint64_t m_totalPRE;
      uint64_t m_totalCAS;
      uint64_t m_totalL;
  };

  //! @struct cmdNode
  //! @brief  Command node for PIM program
  //! @details This structure holds information about a PIM command, including the command type, source and destination objects, and various performance metrics.
  //! @note   This structure is used to represent a command in a PIM prog for PIM fusion.
  

  enum class EventType {
    ACTIVATE_READ,
    ACTIVATE_WRITE,
    READ_SRC1,
    READ_SRC2,
    READ_SCALAR,
    COMPUTE_CHUNK,
    WRITE_CHUNK,
    PRECHARGE_READ,
    PRECHARGE_WRITE
  };

  struct EventNode {
    EventType type;
    uint64_t eventID;
    size_t cmdID;
    unsigned bitsPerElement;
    unsigned chunkID;
    unsigned passId;
    unsigned earliestCycle = 0; // Earliest cycle this event can start
    unsigned stalledCycle = 0; // Cycle when this event is stalled
    std::vector<EventNode*> consumers;
    std::vector<EventNode*>producers;
    bool hasExecuted = false; // Flag to indicate if the event has been executed
    unsigned cycleCount = 0; // Number of cycles required for this event
    double energyConsumed = 0.0; // Energy consumed by this event in mJ 
    std::string toString(pimeval::EventType type) const {
    switch (type) {
        case pimeval::EventType::ACTIVATE_READ: return "ACTIVATE_READ";
        case pimeval::EventType::ACTIVATE_WRITE: return "ACTIVATE_WRITE";
        case pimeval::EventType::READ_SRC1: return "READ_SRC1";
        case pimeval::EventType::READ_SRC2: return "READ_SRC2";
        case pimeval::EventType::COMPUTE_CHUNK: return "COMPUTE";
        case pimeval::EventType::WRITE_CHUNK: return "WRITE_CHUNK";
        case pimeval::EventType::PRECHARGE_READ: return "PRECHARGE_READ";
        case pimeval::EventType::PRECHARGE_WRITE: return "PRECHARGE_WRITE";
        default: return "UNKNOWN";
    }
  }
  void print() const {
    printf("PassID: %u, ChunkID: %u, Type: %s, EventID: %lu\n",
           passId, chunkID, toString(type).c_str(), eventID);
    printf("    Producers: ");
    if (producers.empty()) {
          printf("None");
    } else {
      for (auto p : producers) printf("%lu ", p->eventID);
    }
    printf("\n");

    printf("    Consumers: ");
    if (consumers.empty()) {
        printf("None");
    } else {
        for (auto c : consumers) printf("%lu ", c->eventID);
    }
    printf("\n");
  }
  };

  inline EventNode* generateEvent(EventType type, uint64_t eventID, size_t cmdID,
                                unsigned chunkID, unsigned passID, unsigned bitsPerElement,
                                unsigned cycleCount = 0, double energyConsumed = 0.0) {
    EventNode* event = new EventNode();
    event->type = type;
    event->eventID = eventID;
    event->cmdID = cmdID;
    event->chunkID = chunkID;
    event->passId = passID;
    event->bitsPerElement = bitsPerElement;
    event->cycleCount = cycleCount;
    event->energyConsumed = energyConsumed;
    return event;
  }

  struct cmdNode
  {
    PimCmdEnum cmdType; // PIM command type
    size_t cmdId; // Command ID
    std::vector<pimObjInfo*> dests; // Destination object 
    std::vector<pimObjInfo*> srcs; // More than one source object for 2-operand commands
    std::vector<size_t> producers;  // previous commands this one depends on
    std::vector<size_t> consumers;  // later commands that depend on this one
    unsigned numRead1; // Number of read operation for src1
    unsigned numRead2; // Number of read operation for src2
    unsigned numWrite; // Number of write operations
    bool canPrefetch; // Whether this command can prefetch data for next iteration
    std::unordered_map<unsigned, std::unordered_map<unsigned, std::vector<pimeval::EventNode*>>> events; // Events for this command, indexed by pass and chunk ID
  };

}

//! @class  pimPerfEnergyModelParams
//! @brief  Parameters for creating perf energy models
class pimPerfEnergyModelParams
{
public:
  pimPerfEnergyModelParams(PimDeviceEnum simTarget, unsigned numRanks, const pimParamsDram& paramsDram)
    : m_simTarget(simTarget), m_numRanks(numRanks), m_paramsDram(paramsDram) {}
  PimDeviceEnum getSimTarget() const { return m_simTarget; }
  unsigned getNumRanks() const { return m_numRanks; }
  const pimParamsDram& getParamsDram() const { return m_paramsDram; }
  
  template <typename T>
  void setArchSpecificConfig(std::shared_ptr<T> config) {
    m_archSpecificConfig = config;
  }

  template <typename T>
  std::shared_ptr<T> getArchSpecificConfig() const {
    return std::static_pointer_cast<T>(m_archSpecificConfig);
  }

private:
  PimDeviceEnum m_simTarget;
  unsigned m_numRanks;
  const pimParamsDram& m_paramsDram;
  std::shared_ptr<void> m_archSpecificConfig = nullptr;
};

//! @class  pimPerfEnergyFactory
//! @brief  PIM performance energy model factory
class pimPerfEnergyBase;
class pimPerfEnergyFactory
{
public:
  // pimDevice is not fully constructed at this point. Do not call pimSim::get() in pimPerfEnergyBase ctor.
  static std::unique_ptr<pimPerfEnergyBase> createPerfEnergyModel(const pimPerfEnergyModelParams& params);
};

//! @class  pimPerfEnergyBase
//! @brief  PIM performance energy model base class
class pimPerfEnergyBase
{
public:
  pimPerfEnergyBase(const pimPerfEnergyModelParams& params);
  virtual ~pimPerfEnergyBase() {}

  virtual pimeval::perfEnergy getPerfEnergyForBytesTransfer(PimCmdEnum cmdType, uint64_t numBytes) const;
  virtual pimeval::perfEnergy getPerfEnergyForFunc1(PimCmdEnum cmdType, const pimObjInfo& objSrc, const pimObjInfo& objDest) const;
  virtual pimeval::perfEnergy getPerfEnergyForFunc2(PimCmdEnum cmdType, const pimObjInfo& objSrc1, const pimObjInfo& objSrc2, const pimObjInfo& objDest) const;
  virtual pimeval::perfEnergy getPerfEnergyForReduction(PimCmdEnum cmdType, const pimObjInfo& obj, unsigned numPass) const;
  virtual pimeval::perfEnergy getPerfEnergyForBroadcast(PimCmdEnum cmdType, const pimObjInfo& obj) const;
  virtual pimeval::perfEnergy getPerfEnergyForRotate(PimCmdEnum cmdType, const pimObjInfo& obj) const;
  virtual pimeval::perfEnergy getPerfEnergyForPrefixSum(PimCmdEnum cmdType, const pimObjInfo& obj) const;
  virtual pimeval::perfEnergy getPerfEnergyForMac(PimCmdEnum cmdType, const pimObjInfo& objSrc, const pimObjInfo& objDest) const;
  virtual std::vector<pimeval::perfEnergy> getPerfEnergyForPIMProg(std::vector<pimeval::cmdNode>& cmdGraph) const;

protected:
  PimDeviceEnum m_simTarget;
  unsigned m_numRanks;
  const pimParamsDram& m_paramsDram;

  const double m_nano_to_milli = 1000000.0;
  const double m_pico_to_milli = 1000000000.0;
  double m_tR; // Row read latency in ms
  double m_tW; // Row write latency in ms
  double m_tACT; // Row read(ACT) latency in ms
  double m_tPRE; // Row precharge latency in ms
  double m_tL; // Logic operation for bitserial / tCCD in ms
  double m_tGDL; // Fetch data from local row buffer to global row buffer
  double m_tCAS; // CAS time in ms
  int m_GDLWidth; // Number of bits that can be fetched from local to global row buffer.
  int m_numChipsPerRank; // Number of chips per rank
  double m_typicalRankBW; // typical rank data transfer bandwidth in GB/s

  double m_eAP; // Row read(ACT) energy in mJ microjoule
  double m_eL; // Logic energy in mJ microjoule
  double m_eR_L; // local row buffer to global row buffer
  double m_eW_L; // global row buffer to local row buffer
  double m_eR_S; // local row buffer to global row buffer
  double m_eW_S; // global row buffer to local row buffer
  double m_eACT; // Row activate energy in mJ 
  double m_ePRE; // Row precharge energy in mJ
  double m_pBCore; // background power for each core in W
  double m_pBChip; // background power for each core in W
  double m_tCK; // Clock cycle time in ms
  unsigned m_tCCD_S; // Short command delay in cycles
  unsigned m_tCCD_L; // Long command delay in cycles
  unsigned m_tRCDRD; // RCDRD in cycles
  unsigned m_tRCDWR; // RCDWR in cycles
  unsigned m_tRP; // RP in cycles
  unsigned m_tRAS; // RAS in cycles
};


#endif

