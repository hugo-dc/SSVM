// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "executor/hostfunc.h"
#include "vm/environment.h"
#include <boost/multiprecision/cpp_int.hpp>

namespace SSVM {
namespace Executor {

template <typename T> class EEI : public HostFunction<T> {
public:
  EEI(VM::EVMEnvironment &HostEnv, const std::string &FuncName = "",
      const uint64_t &Cost = 0)
      : HostFunction<T>("ethereum", FuncName, Cost), Env(HostEnv) {}

protected:
  VM::EVMEnvironment &Env;

  /// Helper function of add copy cost.
  ErrCode addCopyCost(VM::EnvironmentManager &EnvMgr, uint64_t Length) {
    uint64_t TakeGas = 3 * ((Length + 31) / 32);
    return EnvMgr.addCost(TakeGas) ? ErrCode::Success : ErrCode::Revert;
  }

  /// Helper function to get max call gas.
  uint64_t getMaxCallGas() {
    return Env.getGasLeft() - (Env.getGasLeft() / 64);
  }

  /// Helper function to load value and store to evmc_uint256be.
  ErrCode loadUInt(Instance::MemoryInstance &MemInst, evmc_uint256be &Dst,
                   uint32_t Off, uint32_t Bytes = 32) {
    if (Bytes > 32) {
      Bytes = 32;
    }
    Dst = {};
    return MemInst.getArray(Dst.bytes + (32 - Bytes), Off, Bytes, true);
  }

  /// Helper function to load evmc_address from memory instance.
  ErrCode loadAddress(Instance::MemoryInstance &MemInst, evmc_address &Dst,
                      uint32_t Off) {
    Dst = {};
    return MemInst.getArray(Dst.bytes, Off, 20);
  }

  /// Helper function to load evmc_bytes32 from memory instance.
  ErrCode loadBytes32(Instance::MemoryInstance &MemInst, evmc_bytes32 &Dst,
                      uint32_t Off) {
    Dst = {};
    return MemInst.getArray(Dst.bytes, Off, 32);
  }

  /// Helper function to reverse and store evmc_uint256be to memory instance.
  ErrCode storeUInt(Instance::MemoryInstance &MemInst,
                    const evmc_uint256be &Src, uint32_t Off,
                    uint32_t Bytes = 32) {
    if (Bytes > 32) {
      Bytes = 32;
    }
    for (uint32_t I = 0; I < 32 - Bytes; ++I) {
      if (Src.bytes[I]) {
        return ErrCode::ExecutionFailed;
      }
    }
    return MemInst.setArray(Src.bytes + (32 - Bytes), Off, Bytes, true);
  }

  /// Helper function to store evmc_address to memory instance.
  ErrCode storeAddress(Instance::MemoryInstance &MemInst,
                       const evmc_address &Addr, uint32_t Off) {
    return MemInst.setArray(Addr.bytes, Off, 20);
  }

  /// Helper function to store evmc_bytes32 to memory instance.
  ErrCode storeBytes32(Instance::MemoryInstance &MemInst,
                       const evmc_bytes32 &Bytes, uint32_t Off) {
    return MemInst.setArray(Bytes.bytes, Off, 32);
  }

  /// Helper function to convert evmc_bytes32 to uint128_t.
  ErrCode convToUInt128(const evmc_uint256be &Src,
                        boost::multiprecision::uint128_t &Dst) {
    for (uint32_t I = 0; I < 16; ++I) {
      if (Src.bytes[I]) {
        return ErrCode::ExecutionFailed;
      }
    }
    for (uint32_t I = 16; I < 32; ++I) {
      Dst <<= 8;
      Dst |= Src.bytes[I];
    }
    return ErrCode::Success;
  }

  /// Helper function to make call operation.
  ErrCode callContract(VM::EnvironmentManager &EnvMgr,
                       Instance::MemoryInstance &MemInst, uint32_t &Ret,
                       evmc_message &Msg, uint32_t DataOffset,
                       uint32_t DataLength, uint32_t CreateResOffset = 0) {
    evmc_context *Cxt = Env.getEVMCContext();

    /// Check depth.
    if (Env.getDepth() >= 1024) {
      Ret = 1;
      return ErrCode::Success;
    }

    /// Setup input data.
    std::vector<uint8_t> Code;
    if (DataLength > 0) {
      if (auto Status = MemInst.getBytes(Code, DataOffset, DataLength);
          Status != ErrCode::Success) {
        return Status;
      }
      Msg.input_data = &Code[0];
      Msg.input_size = Code.size();
    }

    /// Check flag.
    if ((Msg.kind == evmc_call_kind::EVMC_CREATE ||
         (Msg.kind == evmc_call_kind::EVMC_CALL &&
          !evmc::is_zero(Msg.value))) &&
        (Env.getFlag() & evmc_flags::EVMC_STATIC)) {
      return ErrCode::ExecutionFailed;
    }

    /// Take additional gas.
    if ((Msg.kind == evmc_call_kind::EVMC_CALL ||
         Msg.kind == evmc_call_kind::EVMC_CALLCODE) &&
        !evmc::is_zero(Msg.value)) {
      /// Take transfer gas.
      if (!EnvMgr.addCost(9000ULL)) {
        return ErrCode::CostLimitExceeded;
      }

      /// Take gas if create new account.
      if (!Cxt->host->account_exists(Cxt, &(Msg.destination))) {
        if (!EnvMgr.addCost(25000ULL)) {
          return ErrCode::CostLimitExceeded;
        }
      }
    }

    /// Check balance.
    if (((Msg.kind == evmc_call_kind::EVMC_CALL ||
          Msg.kind == evmc_call_kind::EVMC_CALLCODE) &&
         !evmc::is_zero(Msg.value)) ||
        Msg.kind == evmc_call_kind::EVMC_CREATE) {
      boost::multiprecision::uint128_t DstBalance = 0, ValBalance = 0;
      if (convToUInt128(Cxt->host->get_balance(Cxt, &(Msg.sender)),
                        DstBalance) != ErrCode::Success) {
        return ErrCode::ExecutionFailed;
      }
      if (convToUInt128(Msg.value, ValBalance) != ErrCode::Success) {
        return ErrCode::ExecutionFailed;
      }
      if (DstBalance < ValBalance) {
        Ret = 1;
        return ErrCode::Success;
      }
    }

    /// Assign gas to callee. Msg.gas is ensured <= remain gas in caller.
    EnvMgr.addCost(Msg.gas);

    // Add gas stipend for value transfers.
    if (!evmc::is_zero(Msg.value) && Msg.kind != evmc_call_kind::EVMC_CREATE) {
      Msg.gas += 2300ULL;
    }

    /// Call.
    evmc_result CallRes = Cxt->host->call(Cxt, &Msg);

    /// Return left gas.
    if (CallRes.gas_left < 0) {
      return ErrCode::ExecutionFailed;
    }
    EnvMgr.subCost(CallRes.gas_left);

    /// Return data.
    if (Msg.kind == evmc_call_kind::EVMC_CREATE &&
        CallRes.status_code == EVMC_SUCCESS) {
      if (ErrCode Status =
              storeAddress(MemInst, CallRes.create_address, CreateResOffset);
          Status != ErrCode::Success) {
        return Status;
      }
      Env.getReturnData().clear();
    } else if (CallRes.output_data) {
      Env.getReturnData().assign(CallRes.output_data,
                                 CallRes.output_data + CallRes.output_size);
    } else {
      Env.getReturnData().clear();
    }

    /// Return status.
    switch (CallRes.status_code) {
    case evmc_status_code::EVMC_SUCCESS:
      Ret = 0;
      break;
    case evmc_status_code::EVMC_REVERT:
      Ret = 2;
      break;
    default:
      Ret = 1;
      break;
    }
    return ErrCode::Success;
  }
};

} // namespace Executor
} // namespace SSVM
