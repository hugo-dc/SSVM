// SPDX-License-Identifier: Apache-2.0
//===-- ssvm/vm/vm.h - VM execution flow class definition -----------------===//
//
// Part of the SSVM Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file is the definition class of VM class.
///
//===----------------------------------------------------------------------===//
#pragma once

#include "common.h"
#include "configure.h"
#include "environment.h"
#include "envmgr.h"
#include "executor/entry/value.h"
#include "executor/executor.h"
#include "executor/hostfunc.h"
#include "loader/loader.h"
#include "rapidjson/document.h"
#include "result.h"
#include "support/casting.h"
#include "validator/validator.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SSVM {
namespace VM {

namespace {
/// Accept host functions.
template <typename T, typename TR>
using TypeFunc =
    typename std::enable_if_t<std::is_base_of_v<Executor::HostFunctionBase, T>,
                              TR>;
} // namespace

/// VM execution flow class
class VM {
public:
  VM() = delete;
  VM(Configure &InputConfig)
      : Config(InputConfig), EnvMgr(InputConfig), ExecutorEngine(this->EnvMgr),
        InVMStore(nullptr), OutVMStore(nullptr), OutAlloc(nullptr) {}
  ~VM() = default;

  /// Set the wasm file path.
  ErrCode setPath(const std::string &FilePath);

  /// Set the wasm byte code.
  ErrCode setCode(const std::vector<uint8_t> &Code);

  /// Set host function.
  template <typename T>
  TypeFunc<T, ErrCode> setHostFunction(std::unique_ptr<T> &&Func) {
    return setHostFunction<T>(Func);
  }
  template <typename T>
  TypeFunc<T, ErrCode> setHostFunction(std::unique_ptr<T> &Func) {
    std::unique_ptr<Executor::HostFunctionBase> NewFunc = std::move(Func);
    if (ExecutorEngine.setHostFunction(NewFunc) == Executor::ErrCode::Success) {
      return ErrCode::Success;
    }
    return ErrCode::Failed;
  }

  /// Append the start function arguments.
  template <typename T>
  typename std::enable_if_t<Support::IsWasmBuiltInV<T>, ErrCode>
  appendArgument(const T &Val) {
    Args.push_back(Val);
    return ErrCode::Success;
  }
  ErrCode appendArgument(const Executor::Value &Val) {
    Args.push_back(Val);
    return ErrCode::Success;
  }

  /// Get start function return values.
  void getReturnValue(std::vector<Executor::Value> &RetVals) { RetVals = Rets; }

  /// Prepare VM according to VM type before executing wasm.
  void initVMEnv();

  /// Load given wasm file or wasm bytecode
  ErrCode loadWasm();

  /// Validate loaded wasm module
  ErrCode validate();

  /// Set the entry function name
  void setEntryFuncName(const std::string &FuncName);

  /// Instantiate wasm module
  ErrCode instantiate();

  /// Run wasm module with given function name and input.
  ErrCode runWasm();

  /// Clean up VM status
  void cleanup();

  /// Execute wasm with given input.
  ErrCode execute();
  ErrCode execute(const std::string &FuncName);

  /// Return VMResult
  Result getResult() { return VMResult; }

  /// Getter of Environment.
  template <typename T> TypeEnv<T> *getEnvironment(Configure::VMType Type) {
    return EnvMgr.getEnvironment<T>(Type);
  }

  /// Setter of cost limit.
  void setCostLimit(const uint64_t &Limit) { EnvMgr.setCostLimit(Limit); }

  /// Getter of cost limit.
  uint64_t getCostLimit() { return EnvMgr.getCostLimit(); }

  /// Getter of used cost.
  uint64_t getUsedCost() { return EnvMgr.getCostSum(); }

  /// Getter of service name.
  std::string &getServiceName() { return ServiceName; }

  /// Getter of UUID.
  uint64_t &getUUID() { return UUID; }

  /// Set input and output JSON object for saving and restoring VM.
  void setVMStore(rapidjson::Value &InStore, rapidjson::Value &OutStore,
                  rapidjson::Document::AllocatorType &Allocator) {
    InVMStore = &InStore;
    OutVMStore = &OutStore;
    OutAlloc = &Allocator;
  }

  /// Memory helper function
  void setMemoryWithBytes(const std::vector<uint8_t> &Src,
                          const uint32_t DistMemIdx, const uint32_t MemOffset,
                          const uint64_t Size) {
    ExecutorEngine.setMemoryWithBytes(Src, DistMemIdx, MemOffset, Size);
  }
  void getMemoryToBytes(const uint32_t SrcMemIdx, const uint32_t MemOffset,
                        std::vector<uint8_t> &Dist, const uint64_t Size) {
    ExecutorEngine.getMemoryToBytes(SrcMemIdx, MemOffset, Dist, Size);
  }
  void getMemoryToBytesAll(const uint32_t SrcMemIdx, std::vector<uint8_t> &Dist,
                           unsigned int &DataPageSize) {
    ExecutorEngine.getMemoryToBytesAll(SrcMemIdx, Dist, DataPageSize);
  }
  void setMemoryDataPageSize(const uint32_t SrcMemIdx,
                             const unsigned int DataPageSize) {
    ExecutorEngine.setMemoryDataPageSize(SrcMemIdx, DataPageSize);
  }

private:
  /// Functions for running.
  ErrCode runLoader();
  ErrCode runValidator();
  ErrCode runExecutor();

  /// Helper function for inserting host functions according to VM type.
  ErrCode prepareVMHost();

  /// Wasm source.
  std::string WasmPath;
  std::vector<uint8_t> WasmCode;

  /// VM state objects.
  rapidjson::Value *InVMStore;
  rapidjson::Value *OutVMStore;
  rapidjson::Document::AllocatorType *OutAlloc;

  Configure &Config;
  EnvironmentManager EnvMgr;
  Loader::Loader LoaderEngine;
  Executor::Executor ExecutorEngine;
  Validator::Validator ValidatorEngine;
  std::unique_ptr<AST::Module> Mod;
  std::vector<Executor::Value> Args;
  std::vector<Executor::Value> Rets;
  Result VMResult;

  /// Identification
  std::string ServiceName;
  uint64_t UUID;
};

} // namespace VM
} // namespace SSVM
