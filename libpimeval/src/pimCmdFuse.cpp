// File: pimCmdFuse.cpp
// PIMeval Simulator - PIM API Fusion
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "pimSim.h"          // for pimSim
#include "pimSimConfig.h"    // for pimSimConfig
#include "pimDevice.h"       // for pimDevice
#include "pimCore.h"         // for pimCore
#include "pimResMgr.h"       // for pimResMgr
#include "pimCmdFuse.h"
#include <cstdio>
#include <typeinfo>


//! @brief  Pim CMD: PIM API Fusion
bool
pimCmdFuse::execute()
{
  if (m_debugCmds) {
    std::printf("PIM-Cmd: API Fusion\n");
  }
  
  // Functional simulation
  // TODO: skip original updateStats
  bool success = true;
  for (auto& progApi : m_prog.m_apis) {
    if (m_debugCmds)
    {
      std::printf("PIM-API: %s(", pimUtils::pimApiToStr(progApi.m_funcPtr).c_str());
      for (size_t i = 0; i < progApi.m_args.size(); ++i)
      {
        const std::any &arg = progApi.m_args[i];
        if (arg.type() == typeid(PimObjId)) {
          std::printf("PimObjId(%d)", std::any_cast<PimObjId>(arg));
        }
        if (i + 1 < progApi.m_args.size())
          std::printf(", ");
      }
      std::printf(")\n");
    }
    PimStatus status = progApi.m_api();
    if (status != PIM_OK) {
      std::printf("PIM-Error: Functional execution failed for PIM CMD: %s. \n", pimUtils::pimApiToStr(progApi.m_funcPtr).c_str());
      success = false;
      break;
    }
  }
  // Analyze API fusion opportunities
  success = success && updateStats();
  return success;
}

//! @brief  Pim CMD: PIM API Fusion - update stats
bool
pimCmdFuse::updateStats() const
{
  if (m_debugCmds) {
    std::printf("PIM-Fuse: Update Stats\n");
  }
  std::printf("PIM-Info: Starting Dependency Graph Construction for commands.\n");

  std::vector<pimeval::cmdNode> depGraph;
  std::unordered_map<PimObjId, size_t> lastWriter;

  uint64_t i = 0;
  for (const auto& progApi : m_prog.m_apis) {
    PimCmdEnum cmdType = pimAPItoPimCmdEnum(progApi.m_funcPtr);
    pimeval::cmdNode node = cmdTupleToNode(cmdType, progApi, i);
    
    std::set<size_t> seen; // Avoid duplicate dependencies

    // RAW dependencies (read-after-write)
    for (const auto& src : node.srcs) {
      if (lastWriter.count(src->getObjId()) && src->getObjId() != -1) {
        size_t prodId = lastWriter[src->getObjId()];
        if (seen.insert(prodId).second) {
          node.producers.push_back(prodId);
          depGraph[prodId].consumers.push_back(i);
        }
      }
    }

    if (!node.dests.empty()) { 
      // WAW dependency (write-after-write)
      if (lastWriter.count(node.dests[0]->getObjId())) {
        size_t priorWriter = lastWriter[node.dests[0]->getObjId()];
        if (seen.insert(priorWriter).second) {
          node.producers.push_back(priorWriter);
          depGraph[priorWriter].consumers.push_back(i);
        }
      }
      // Update writer map
      lastWriter[node.dests[0]->getObjId()] = i;
    }
    depGraph.push_back(node);
    ++i;
  }
  lastWriter.clear();

  if (m_debugCmds) {
    std::printf("PIM-INFO: %zu commands in dependency graph.\n", depGraph.size());
    for (const auto& node : depGraph) {
      printf("Cmd ID %zu (%s):\n", node.cmdId, pimCmd::getName(node.cmdType, "").c_str());
      printf("  Srcs: ");
      for (const auto& src : node.srcs) {
        printf("%d ", src->getObjId());
      }
      printf("\n");
      printf("  Dest: ");
      if (!node.dests.empty()) printf("%d", node.dests[0]->getObjId());
      printf("\n");

      printf("  Producers: ");
      for (const auto& pid : node.producers) {
        printf("%zu ", pid);
      }
      printf("\n");
      printf("  Consumers: ");
      for (const auto& cid : node.consumers) {
        printf("%zu ", cid);
      }
      printf("\n\n");
    }
  }

  std::vector<pimeval::perfEnergy> perfEnergies = m_device->getPerfEnergyModel()->getPerfEnergyForPIMProg(depGraph);
  if (perfEnergies.empty()) {
    std::printf("PIM-Error: No performance energy data available for PIM Prog.\n");
    return false;
  }
  for (size_t idx = 0; idx < depGraph.size(); ++idx) {
    const auto& cmdNode = depGraph[idx];
    if (cmdNode.cmdType == PimCmdEnum::NOOP || cmdNode.cmdType == PimCmdEnum::COPY_H2D || cmdNode.cmdType == PimCmdEnum::COPY_D2H || cmdNode.cmdType == PimCmdEnum::COPY_D2D) continue;
    const pimObjInfo& obj = !cmdNode.srcs.empty() ? *cmdNode.srcs[0] : *cmdNode.dests[0];
    pimSim::get()->getStatsMgr()->recordCmd(getName(cmdNode.cmdType, obj.getDataType(), obj.isVLayout(), true), perfEnergies[idx]);
  }
  
  // Clean up EventNode pointers to prevent memory leaks
  for (auto& node : depGraph) {
    for (auto& passMap : node.events) {
      for (auto& chunkMap : passMap.second) {
        for (auto* eventNode : chunkMap.second) {
          delete eventNode;
        }
        chunkMap.second.clear();
      }
      passMap.second.clear();
    }
    node.events.clear();
  }
  
  return true;
}