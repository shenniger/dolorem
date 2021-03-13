#ifndef LLVMEXT_H
#define LLVMEXT_H

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This function adds a global symbol which is zero-initialized.
 *
 * llvm-c does allow to add an initialized global, but its interface is a
 * bit clumsy due to the missing llvm::as<>.
 */
void AddGlobalDef(LLVMModuleRef m, LLVMTypeRef ty, const char *name);

long GetTypeSize(LLVMModuleRef m, LLVMTypeRef ty);

void *GetBuilderPosition(LLVMBuilderRef b);
void SetBuilderPosition(LLVMBuilderRef b, void *pos); /* consumes pos */

#ifdef __cplusplus
}
#endif

#endif
