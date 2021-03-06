// SPDX-License-Identifier: Apache-2.0
//===-- ssvm/executor/common.h - Common definitions in Executor -*- C++ -*-===//
//
// Part of the SSVM Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the common-use definitions in Executor.
///
//===----------------------------------------------------------------------===//
#pragma once

#define TIMER_TAG_EXECUTION 1U
#define TIMER_TAG_HOSTFUNC 2U
#ifdef ONNC_WASM
#define TIMER_TAG_QITC_INFER_SSVM 256U
#define TIMER_TAG_QITC_INFER_HOST 257U
#endif

namespace SSVM {
namespace Executor {

/// Executor error code enum class.
enum class ErrCode : unsigned int {
  Success = 0,
  WrongExecutorFlow,    /// Wrong executor's flow
  WrongWorkerFlow,      /// Wrong worker's flow
  InstantiateFailed,    /// Fail when instantiating
  WrongInstanceAddress, /// Wrong access of instances
  ExecutionFailed,      /// Runtime error when executing
  TypeNotMatch, /// Value type between instructions and stack or store not match
  StackWrongEntry,         /// Entry type not match when get or pop entry
  StackEmpty,              /// Empry stack when get or pop entry
  WrongLocalAddress,       /// Wrong access of local variable
  InstructionTypeMismatch, /// Instruction type not match
  DivideByZero,            /// Divide by zero
  FloatPointException,     /// Floating point exception
  CastingError,            /// Cannot do type casting
  Unimplemented,           /// Instruction is unimplemented
  AccessForbidMemory,      /// Access the forbid memory section
  SliceDataFailed,         /// Fail to get slice from memory.data
  Unreachable,             /// Get a unreachable instruction
  WrongInstructionCounter, /// Instruction counter error
  TableSizeExceeded,       /// Exceeded limit of table size in table instance.
  MemorySizeExceeded,      /// Exceeded limit of memory page in memory instance.
  FunctionInvalid,         /// Invalid operation to function instance.
  CallFunctionError,       /// Arguement not match function type.
  Revert,                  /// Revert by evm.
  CostLimitExceeded,       /// Exceeded cost limit (out of gas).
  Terminated               /// Forced terminated by program and return success.
};

} // namespace Executor
} // namespace SSVM
