// SPDX-License-Identifier: Apache-2.0
//===-- ssvm/executor/hostfuncmgr.h - host function manager class ---------===//
//
// Part of the SSVM Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of host function manager.
///
//===----------------------------------------------------------------------===//
#pragma once

#include "common.h"
#include "hostfunc.h"

namespace SSVM {
namespace Executor {

class HostFunctionManager {
public:
  HostFunctionManager() = default;
  ~HostFunctionManager() = default;

  /// Insert new host function and get index.
  ErrCode insertHostFunction(std::unique_ptr<HostFunctionBase> &Func,
                             unsigned int &NewAddr) {
    NewAddr = HostFuncs.size();
    HostFuncs.push_back(std::move(Func));
    return ErrCode::Success;
  }

  /// Get host function by address.
  ErrCode getHostFunction(unsigned int Addr, HostFunctionBase *&Func) {
    if (Addr >= HostFuncs.size()) {
      return ErrCode::WrongInstanceAddress;
    }
    Func = HostFuncs[Addr].get();
    return ErrCode::Success;
  }

  /// Reset host function manager.
  ErrCode reset() {
    HostFuncs.clear();
    return ErrCode::Success;
  }

private:
  std::vector<std::unique_ptr<HostFunctionBase>> HostFuncs;
};

} // namespace Executor
} // namespace SSVM
