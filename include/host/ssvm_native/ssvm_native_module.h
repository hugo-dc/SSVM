// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "runtime/importobj.h"

namespace SSVM {
namespace Host {

class SSVMNativeModule : public Runtime::ImportObject {
public:
  SSVMNativeModule();
  virtual ~SSVMNativeModule() = default;
};

} // namespace Host
} // namespace SSVM