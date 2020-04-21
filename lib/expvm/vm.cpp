#include "expvm/vm.h"
#include "host/ethereum/eeimodule.h"
#include "host/wasi/wasimodule.h"

#ifdef ONNC_WASM
#include "host/onnc/onncmodule.h"
#endif

#include <time.h>

namespace SSVM {
namespace ExpVM {

VM::VM(Configure &InputConfig)
    : Config(InputConfig), Stage(VMStage::Inited), InterpreterEngine(&Measure),
      Store(std::make_unique<Runtime::StoreManager>()), StoreRef(*Store.get()) {
  initVM();
}

VM::VM(Configure &InputConfig, Runtime::StoreManager &S)
    : Config(InputConfig), Stage(VMStage::Inited), InterpreterEngine(&Measure),
      StoreRef(S) {
  initVM();
}

void VM::initVM() {
  /// Set cost table and create import modules from configure.
  CostTab.setCostTable(Configure::VMType::Wasm);
  Measure.setCostTable(CostTab.getCostTable(Configure::VMType::Wasm));
  if (Config.hasVMType(Configure::VMType::Wasi)) {
    /// 2nd priority of cost table: Wasi
    std::unique_ptr<Runtime::ImportObject> WasiMod =
        std::make_unique<Host::WasiModule>();
    InterpreterEngine.registerModule(StoreRef, *WasiMod.get());
    ImpObjs.insert({Configure::VMType::Wasi, std::move(WasiMod)});
    CostTab.setCostTable(Configure::VMType::Wasi);
    Measure.setCostTable(CostTab.getCostTable(Configure::VMType::Wasi));
  }
  if (Config.hasVMType(Configure::VMType::Ewasm)) {
    /// 1st priority of cost table: EWasm
    std::unique_ptr<Runtime::ImportObject> EEIMod =
        std::make_unique<Host::EEIModule>(Measure.getCostLimit(),
                                          Measure.getCostSum());
    InterpreterEngine.registerModule(StoreRef, *EEIMod.get());
    ImpObjs.insert({Configure::VMType::Ewasm, std::move(EEIMod)});
    CostTab.setCostTable(Configure::VMType::Ewasm);
    Measure.setCostTable(CostTab.getCostTable(Configure::VMType::Ewasm));
  }
#ifdef ONNC_WASM
  if (Config.hasVMType(Configure::VMType::ONNC)) {
    std::unique_ptr<Runtime::ImportObject> ONNCMod =
        std::make_unique<Host::ONNCModule>();
    InterpreterEngine.registerModule(StoreRef, *ONNCMod.get());
    ImpObjs.insert({Configure::VMType::ONNC, std::move(ONNCMod)});
  }
#endif
}

Expect<void> VM::registerModule(const std::string &Name,
                                const std::string &Path) {
  if (Stage == VMStage::Instantiated) {
    /// When registering module, instantiated module in store will be reset.
    /// Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  /// Load module.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    return registerModule(Name, *(*Res).get());
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::registerModule(const std::string &Name, const Bytes &Code) {
  if (Stage == VMStage::Instantiated) {
    /// When registering module, instantiated module in store will be reset.
    /// Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  /// Load module.
  std::unique_ptr<AST::Module> LoadedMod;
  if (auto Res = LoaderEngine.parseModule(Code)) {
    return registerModule(Name, *(*Res).get());
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::registerModule(const Runtime::ImportObject &Obj) {
  if (Stage == VMStage::Instantiated) {
    /// When registering module, instantiated module in store will be reset.
    /// Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  return InterpreterEngine.registerModule(StoreRef, Obj);
}

Expect<void> VM::registerModule(const std::string &Name,
                                const AST::Module &Module) {
  /// Validate module.
  if (auto Res = ValidatorEngine.validate(Module); !Res) {
    return Unexpect(Res);
  }
  return InterpreterEngine.registerModule(StoreRef, Module, Name);
}

Expect<std::vector<ValVariant>>
VM::runWasmFile(const std::string &Path, const std::string &Func,
                const std::vector<ValVariant> &Params) {
  if (Stage == VMStage::Instantiated) {
    /// When running another module, instantiated module in store will be reset.
    /// Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  /// Load module.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    return runWasmFile(*(*Res).get(), Func, Params);
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<ValVariant>>
VM::runWasmFile(const Bytes &Code, const std::string &Func,
                const std::vector<ValVariant> &Params) {
  if (Stage == VMStage::Instantiated) {
    /// When running another module, instantiated module in store will be reset.
    /// Therefore the instantiation should restart.
    Stage = VMStage::Validated;
  }
  /// Load module.
  if (auto Res = LoaderEngine.parseModule(Code)) {
    return runWasmFile(*(*Res).get(), Func, Params);
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<ValVariant>>
VM::runWasmFile(const AST::Module &Module, const std::string &Func,
                const std::vector<ValVariant> &Params) {
  if (auto Res = ValidatorEngine.validate(Module); !Res) {
    return Unexpect(Res);
  }
  struct timespec requestStart, requestEnd;
  clock_gettime(CLOCK_REALTIME, &requestStart);
  if (auto Res = InterpreterEngine.instantiateModule(StoreRef, Module); !Res) {
    return Unexpect(Res);
  }
  clock_gettime(CLOCK_REALTIME, &requestEnd);
  double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
	  + ( requestEnd.tv_nsec - requestStart.tv_nsec )
	  / 1E9;
  printf("Instantiation time: %1fs", accum);
  if (auto Res = InterpreterEngine.invoke(StoreRef, Func, Params)) {
    return *Res;
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::loadWasm(const std::string &Path) {
  /// If not load successfully, the previous status will be reserved.
  if (auto Res = LoaderEngine.parseModule(Path)) {
    Mod = std::move(*Res);
    Stage = VMStage::Loaded;
  } else {
    return Unexpect(Res);
  }
  return {};
}

Expect<void> VM::loadWasm(const Bytes &Code) {
  /// If not load successfully, the previous status will be reserved.
  if (auto Res = LoaderEngine.parseModule(Code)) {
    Mod = std::move(*Res);
    Stage = VMStage::Loaded;
  } else {
    return Unexpect(Res);
  }
  return {};
}

Expect<void> VM::validate() {
  if (Stage < VMStage::Loaded) {
    /// When module is not loaded, not validate.
    return Unexpect(ErrCode::WrongVMWorkflow);
  }
  if (auto Res = ValidatorEngine.validate(*Mod.get())) {
    Stage = VMStage::Validated;
    return {};
  } else {
    return Unexpect(Res);
  }
}

Expect<void> VM::instantiate() {
  if (Stage < VMStage::Validated) {
    /// When module is not validated, not instantiate.
    return Unexpect(ErrCode::ValidationFailed);
  }
  if (auto Res =
          InterpreterEngine.instantiateModule(StoreRef, *Mod.get(), "")) {
    Stage = VMStage::Instantiated;
    return {};
  } else {
    return Unexpect(Res);
  }
}

Expect<std::vector<ValVariant>>
VM::execute(const std::string &Func, const std::vector<ValVariant> &Params) {
  /// Error handling is included in interpreter.
  return InterpreterEngine.invoke(StoreRef, Func, Params);
}

void VM::cleanup() {
  Mod.reset();
  StoreRef.reset();
  Measure.clear();
  Stage = VMStage::Inited;
}

std::vector<std::pair<std::string, Runtime::Instance::FType>>
VM::getFunctionList() const {
  std::vector<std::pair<std::string, Runtime::Instance::FType>> Res;
  for (auto &&Func : StoreRef.getFuncExports()) {
    const auto *FuncInst = *StoreRef.getFunction(Func.second);
    const auto &FuncType = FuncInst->getFuncType();
    Res.push_back({Func.first, FuncType});
  }
  return Res;
}

Runtime::ImportObject *VM::getImportModule(const Configure::VMType Type) {
  if (ImpObjs.find(Type) != ImpObjs.cend()) {
    return ImpObjs[Type].get();
  }
  return nullptr;
}

} // namespace ExpVM
} // namespace SSVM