#include "llvmext.h"
#include <assert.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

using namespace llvm;

void AddGlobalDef(LLVMModuleRef m, LLVMTypeRef ty, const char *name) {
  Type *t = unwrap(ty);
  Constant *init;
  if (t->isAggregateType()) {
    init = ConstantAggregateZero::get(t);
  } else if (t->isIntegerTy()) {
    init = ConstantInt::get(t, 0);
  } else if (t->isPointerTy()) {
    init = ConstantPointerNull::get(cast<PointerType>(t));
  }

  new GlobalVariable(*unwrap(m), t, false, GlobalValue::CommonLinkage, init,
                     name);
}
