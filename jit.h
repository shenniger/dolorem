#ifndef JIT_H
#define JIT_H

#include "hashmap.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Linker.h>
#include <llvm-c/OrcBindings.h>
#include <llvm-c/Target.h>

extern LLVMModuleRef mod, typemod;
extern LLVMOrcJITStackRef orcref;
extern LLVMOrcModuleHandle modhdl;
extern LLVMBuilderRef bldr;
extern void *dlhdl;
extern LLVMTargetMachineRef tm;
extern LLVMOrcSymbolResolverFn orcresolver;
extern map_t map_modules;
extern int dump_modules, dump_lists;

void handle_llvm_error(LLVMErrorRef e);
struct fun;
uint64_t resolve_sym(struct fun *a);
void init_jit();
void begin_new_function();
void end_function(const char *name);
void end_jit();
void add_symbol_to_module(const char *name, LLVMModuleRef dest);
void copy_symbol_to_module(const char *name, LLVMModuleRef dest);
void add_global_symbol(const char *name, LLVMTypeRef t);

#endif
