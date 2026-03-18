// File: pimCmdFuse.h
// PIMeval Simulator - PIM API Fusion
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef LAVA_PIM_CMD_FUSE_H
#define LAVA_PIM_CMD_FUSE_H

#include "libpimeval.h"
#include "pimCmd.h"


//! @class  pimCmdFuse
//! @brief  Pim CMD: PIM API Fusion
class pimCmdFuse : public pimCmd
{
public:
  pimCmdFuse(PimFusionBlock prog) : pimCmd(PimCmdEnum::NOOP), m_prog(prog) {}
  virtual ~pimCmdFuse() {}
  virtual bool execute() override;
  virtual bool updateStats() const override;
private:
  PimFusionBlock m_prog;
  inline PimCmdEnum pimAPItoPimCmdEnum(void* apiPtr) const {
    if (apiPtr == reinterpret_cast<void*>(&pimCopyHostToDevice)) {
      return PimCmdEnum::COPY_H2D;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCopyDeviceToHost)) {
      return PimCmdEnum::COPY_D2H;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCopyDeviceToDevice)) {
      return PimCmdEnum::COPY_D2D;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCopyObjectToObject)) {
      return PimCmdEnum::COPY_O2O;
    } else if (apiPtr == reinterpret_cast<void *>(&pimConvertType)) {
      return PimCmdEnum::CONVERT_TYPE;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAdd)) {
      return PimCmdEnum::ADD;
    } else if (apiPtr == reinterpret_cast<void *>(&pimSub)) {
      return PimCmdEnum::SUB; 
    } else if (apiPtr == reinterpret_cast<void *>(&pimMul)) {
      return PimCmdEnum::MUL;
    } else if (apiPtr == reinterpret_cast<void *>(&pimDiv)) {
      return PimCmdEnum::DIV;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAbs)) {
      return PimCmdEnum::ABS;
    } else if (apiPtr == reinterpret_cast<void *>(&pimNot)) {
      return PimCmdEnum::NOT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAnd)) {
      return PimCmdEnum::AND;
    } else if (apiPtr == reinterpret_cast<void *>(&pimOr)) {
      return PimCmdEnum::OR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimXor)) {
      return PimCmdEnum::XOR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimXnor)) {
      return PimCmdEnum::XNOR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMin)) {
      return PimCmdEnum::MIN;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMax)) {
      return PimCmdEnum::MAX;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAddScalar)) {
      return PimCmdEnum::ADD_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimSubScalar)) {
      return PimCmdEnum::SUB_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMulScalar)) {
      return PimCmdEnum::MUL_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimDivScalar)) {
      return PimCmdEnum::DIV_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAndScalar)) {
      return PimCmdEnum::AND_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimOrScalar)) {
      return PimCmdEnum::OR_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimXorScalar)) {
      return PimCmdEnum::XOR_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimXnorScalar)) {
      return PimCmdEnum::XNOR_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMinScalar)) {
      return PimCmdEnum::MIN_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMaxScalar)) {
      return PimCmdEnum::MAX_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimGT)) {
      return PimCmdEnum::GT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimLT)) {
      return PimCmdEnum::LT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimEQ)) {
      return PimCmdEnum::EQ;
    } else if (apiPtr == reinterpret_cast<void *>(&pimNE)) {
      return PimCmdEnum::NE;
    } else if (apiPtr == reinterpret_cast<void *>(&pimGTScalar)) {
      return PimCmdEnum::GT_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimLTScalar)) {
      return PimCmdEnum::LT_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimEQScalar)) {
      return PimCmdEnum::EQ_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimNEScalar)) {
      return PimCmdEnum::NE_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimScaledAdd)) {
      return PimCmdEnum::SCALED_ADD;
    } else if (apiPtr == reinterpret_cast<void *>(&pimPopCount)) {
      return PimCmdEnum::POPCOUNT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimPrefixSum)) {
      return PimCmdEnum::PREFIX_SUM;
    } else if (apiPtr == reinterpret_cast<void *>(&pimRedSum)) {
      return PimCmdEnum::REDSUM;
    } else if (apiPtr == reinterpret_cast<void *>(&pimRedMin)) {
      return PimCmdEnum::REDMIN;
    } else if (apiPtr == reinterpret_cast<void *>(&pimRedMax)) {
      return PimCmdEnum::REDMAX;
    } else if (apiPtr == reinterpret_cast<void *>(&pimBitSliceExtract)){
      return PimCmdEnum::BIT_SLICE_EXTRACT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimBitSliceInsert)) {
      return PimCmdEnum::BIT_SLICE_INSERT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCondCopy)) {
      return PimCmdEnum::COND_COPY;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCondBroadcast)) {
      return PimCmdEnum::COND_BROADCAST;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCondSelect)) {
      return PimCmdEnum::COND_SELECT;
    } else if (apiPtr == reinterpret_cast<void *>(&pimCondSelectScalar)) {
      return PimCmdEnum::COND_SELECT_SCALAR;
    } else if (apiPtr == reinterpret_cast<void *>(&pimBroadcastInt)) {
      return PimCmdEnum::BROADCAST;
    } else if (apiPtr == reinterpret_cast<void *>(&pimBroadcastUInt)) {
      return PimCmdEnum::BROADCAST;
    } else if (apiPtr == reinterpret_cast<void *>(&pimBroadcastFP)) {
      return PimCmdEnum::BROADCAST;
    } else if (apiPtr == reinterpret_cast<void *>(&pimRotateElementsRight)) {
      return PimCmdEnum::ROTATE_ELEM_R;
    } else if (apiPtr == reinterpret_cast<void *>(&pimRotateElementsLeft)) {
      return PimCmdEnum::ROTATE_ELEM_L;
    } else if (apiPtr == reinterpret_cast<void *>(&pimShiftElementsRight)) {
      return PimCmdEnum::SHIFT_ELEM_R;
    } else if (apiPtr == reinterpret_cast<void *>(&pimShiftElementsLeft)) {
      return PimCmdEnum::SHIFT_ELEM_L;
    } else if (apiPtr == reinterpret_cast<void *>(&pimShiftBitsRight)) {
      return PimCmdEnum::SHIFT_BITS_R;
    } else if (apiPtr == reinterpret_cast<void *>(&pimShiftBitsLeft)) {
      return PimCmdEnum::SHIFT_BITS_L;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAesSbox)) {
      return PimCmdEnum::AES_SBOX;
    } else if (apiPtr == reinterpret_cast<void *>(&pimAesInverseSbox)) {
      return PimCmdEnum::AES_INVERSE_SBOX;
    } else if (apiPtr == reinterpret_cast<void *>(&pimMAC)) {
      return PimCmdEnum::MAC;
    } else {
      std::printf("PIM-Error: Unrecognized PIM Command.\n");
      return PimCmdEnum::NOOP; // Default case for unknown API
    }
  }

  inline pimeval::cmdNode cmdTupleToNode(PimCmdEnum cmdType, const PimApi& progAPI, size_t idx) const {
   std::vector<pimObjInfo*> objPtrs;
    for (const auto& arg : progAPI.m_args) {
      if (arg.type() == typeid(PimObjId)) {
        const pimObjInfo& obj = m_device->getResMgr()->getObjInfo(std::any_cast<PimObjId>(arg));
        objPtrs.push_back(const_cast<pimObjInfo*>(&obj));
      }
   }
   switch (cmdType) {
      case PimCmdEnum::COPY_H2D:
      case PimCmdEnum::BROADCAST:
      case PimCmdEnum::COND_BROADCAST:
      {
        return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {objPtrs[0]},
          .srcs = {},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::COPY_D2H:
      {
          return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {},
          .srcs = {objPtrs[0]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::COPY_D2D:
      case PimCmdEnum::COPY_O2O:
      case PimCmdEnum::COND_COPY:
      case PimCmdEnum::ABS:
      case PimCmdEnum::NOT:
      case PimCmdEnum::POPCOUNT:
      case PimCmdEnum::ADD_SCALAR:
      case PimCmdEnum::SUB_SCALAR:
      case PimCmdEnum::MUL_SCALAR:
      case PimCmdEnum::DIV_SCALAR:
      case PimCmdEnum::AND_SCALAR:
      case PimCmdEnum::OR_SCALAR:
      case PimCmdEnum::XOR_SCALAR:
      case PimCmdEnum::XNOR_SCALAR:
      case PimCmdEnum::MIN_SCALAR:
      case PimCmdEnum::MAX_SCALAR:
      case PimCmdEnum::GT_SCALAR:
      case PimCmdEnum::LT_SCALAR:
      case PimCmdEnum::EQ_SCALAR:
      case PimCmdEnum::NE_SCALAR:
      case PimCmdEnum::BIT_SLICE_EXTRACT:
      case PimCmdEnum::BIT_SLICE_INSERT:
      case PimCmdEnum::CONVERT_TYPE:
      case PimCmdEnum::SHIFT_BITS_L:
      case PimCmdEnum::SHIFT_BITS_R:
      case PimCmdEnum::COND_SELECT_SCALAR:
      {
          return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {objPtrs[1]},
          .srcs = {objPtrs[0]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::ADD:
      case PimCmdEnum::SUB:
      case PimCmdEnum::MUL:
      case PimCmdEnum::DIV:
      case PimCmdEnum::AND:
      case PimCmdEnum::OR:
      case PimCmdEnum::XOR:
      case PimCmdEnum::XNOR:
      case PimCmdEnum::MIN:
      case PimCmdEnum::MAX:
      case PimCmdEnum::GT:
      case PimCmdEnum::LT:
      case PimCmdEnum::EQ:
      case PimCmdEnum::NE:
      case PimCmdEnum::SCALED_ADD:
      {
          return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {objPtrs[2]},
          .srcs = {objPtrs[0], objPtrs[1]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::COND_SELECT:
      {
          return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {objPtrs[3]},
          .srcs = {objPtrs[0], objPtrs[1], objPtrs[2]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::REDSUM:
      case PimCmdEnum::REDMIN:
      case PimCmdEnum::REDMAX:
      case PimCmdEnum::REDSUM_RANGE:
      case PimCmdEnum::REDMIN_RANGE:
      case PimCmdEnum::REDMAX_RANGE:
      {
        return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {},
          .srcs = {objPtrs[0]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      case PimCmdEnum::MAC:
      {
          return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {},
          .srcs = {objPtrs[0], objPtrs[1]},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
      default:
      {
        return pimeval::cmdNode {
          .cmdType = cmdType,
          .cmdId = idx,
          .dests = {},
          .srcs = {},
          .producers = {},
          .consumers = {},
          .numRead1 = 0,
          .numRead2 = 0,
          .numWrite = 0,
          .canPrefetch = false,
          .events = {}
        };
      }
    }
  }

};

#endif

