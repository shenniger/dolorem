#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/ObjectCache.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include <llvm-c/Core.h>
#include <llvm/ExecutionEngine/Orc/Mangling.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <memory>

using namespace llvm;
using namespace orc;

namespace {
class JITHelper {
private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
  void (*pleaseRegisterCallback)(const char *name);

public:
  Error init(void (*pleaseRegisterCallback)(const char *name));
  llvm::Expected<uint64_t> getAddressFor(const std::string &Symbol);
  Error registerAddress(const std::string &Symbol, uint64_t addr);
  Error addModule(Module *m);
};

Error JITHelper::init(void (*pleaseRegisterCallback)(const char *name)) {
  this->pleaseRegisterCallback = pleaseRegisterCallback;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  auto J =
      orc::LLJITBuilder()
          .setCompileFunctionCreator(
              [&](JITTargetMachineBuilder JTMB)
                  -> Expected<std::unique_ptr<IRCompileLayer::IRCompiler>> {
                auto TM = JTMB.createTargetMachine();
                if (!TM)
                  return TM.takeError();
                return std::make_unique<TMOwningSimpleCompiler>(std::move(*TM),
                                                                nullptr);
              })
          .setObjectLinkingLayerCreator([&](ExecutionSession &ES,
                                            const Triple &TT)
                                            -> std::unique_ptr<ObjectLayer> {
            auto GetMemMgr = []() {
              return std::make_unique<SectionMemoryManager>();
            };
            auto ObjLinkingLayer = std::make_unique<RTDyldObjectLinkingLayer>(
                ES, std::move(GetMemMgr));
            if (TT.isOSBinFormatCOFF()) {
              ObjLinkingLayer->setOverrideObjectFlagsWithResponsibilityFlags(
                  true);
              ObjLinkingLayer->setAutoClaimResponsibilityForObjectSymbols(true);
            }
            ObjLinkingLayer->registerJITEventListener(
                *JITEventListener::createGDBRegistrationListener());
            return ObjLinkingLayer;
          })
          .create();
  if (!J)
    return J.takeError();
  jit_.reset(J.get().release());
  return Error::success();
}

Error JITHelper::addModule(Module *M) {
  auto Ctx = std::make_unique<LLVMContext>();
  // register all symbols in module
  // TODO: Solve this better. It is fairly wasteful and might be slightly
  // incorrect in some circumstances.
  for (const Function &f : *M) {
    for (const BasicBlock &b : f) {
      for (const Instruction &i : b) {
        for (const Value *v : i.operand_values()) {
          if (v) {
            if (isa<GlobalValue>(v) &&
                v->getName().substr(0, 11) != "<anonymous-") {
              pleaseRegisterCallback(v->getName().data());
            }
          }
        }
      }
    }
  }
  ThreadSafeModule TSM(std::unique_ptr<Module>(M), std::move(Ctx));
  if (auto err = jit_->addIRModule(std::move(TSM))) {
    return err;
  }
  return Error::success();
}

Expected<uint64_t> JITHelper::getAddressFor(const std::string &Symbol) {
  auto SA = jit_->lookup(Symbol);
  if (auto err = SA.takeError()) {
    return err;
  }

  return reinterpret_cast<uint64_t>((*SA).getAddress());
}

Error JITHelper::registerAddress(const std::string &Symbol, uint64_t addr) {
  MangleAndInterner mangler(jit_->getExecutionSession(), jit_->getDataLayout());
  return jit_->getMainJITDylib().define(absoluteSymbols(
      SymbolMap{{mangler(Symbol),
                 JITEvaluatedSymbol(pointerToJITTargetAddress((void (*)())addr),
                                    JITSymbolFlags::Callable)}}));
  // TODO: This works, but is a hack! We should not just assume every symbol
  // is callable.
}

JITHelper helper;
} // namespace

extern "C" {
LLVMErrorRef InitLLJIT(void (*pleaseRegisterCallback)(const char *)) {
  return wrap(helper.init(pleaseRegisterCallback));
}
LLVMErrorRef AddLLJITModule(LLVMModuleRef M) {
  return wrap(helper.addModule(unwrap(M)));
}
LLVMErrorRef GetAddressFor(const char *s, uint64_t *addr) {
  *addr = 0;
  auto r = helper.getAddressFor(s);
  if (!r) {
    return wrap(r.takeError());
  }
  *addr = r.get();
  return wrap(Error::success());
}
LLVMErrorRef RegisterAddress(const char *s, uint64_t addr) {
  return wrap(helper.registerAddress(s, addr));
}
}
