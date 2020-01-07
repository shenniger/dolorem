#include "jit.h"

#include "list.h"
#include "main.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

#include "llvmext.h"

LLVMModuleRef mod;
LLVMOrcJITStackRef orcref;
LLVMOrcModuleHandle modhdl;
LLVMBuilderRef bldr;
LLVMTargetMachineRef tm;
void *dlhdl;
LLVMOrcSymbolResolverFn orcresolver;
map_t map_modules;

void handle_llvm_error(LLVMErrorRef e) {
  if (e) {
    char *m;
    m = LLVMGetErrorMessage(e);
    compiler_error_internal("LLVM error: %s", m);
  }
}

uint64_t resolve_sym(const char *name) {
  LLVMOrcTargetAddress r;
  handle_llvm_error(LLVMOrcGetSymbolAddress(orcref, &r, name));
  if (!r) {
    if (!((r = (uint64_t)(uintptr_t)dlsym(dlhdl, name)))) {
      compiler_error_internal("couldn't resolve function symbol in JIT: \"%s\"",
                              name);
    }
  }
  return (uint64_t)(uintptr_t)r;
}

static inline uint64_t orcresolverfun(const char *name, void *ctx) {
  (void)ctx;
  return resolve_sym(name);
}
void begin_new_function() { mod = LLVMModuleCreateWithName("test"); }
void end_function(const char *name) {
  LLVMVerifyModule(mod, LLVMPrintMessageAction, NULL);
  if (dump_modules) {
    LLVMDumpModule(mod);
    fputs("=============================================================\n",
          stderr);
  }
  if (hashmap_put(map_modules, name, LLVMCloneModule(mod)) != MAP_OK) {
    compiler_error_internal(
        "couldn't add module to list of modules upon compiling \"%s\"", name);
  }
  handle_llvm_error(
      LLVMOrcAddEagerlyCompiledIR(orcref, &modhdl, mod, orcresolver, NULL));
  mod = NULL;
}
void add_global_symbol(const char *name, LLVMTypeRef t) {
  LLVMModuleRef a, b;
  a = LLVMModuleCreateWithName("test");
  b = LLVMModuleCreateWithName("test");
  AddGlobalDef(a, t, name);
  AddGlobalDef(b, t, name);
  LLVMVerifyModule(a, LLVMPrintMessageAction, NULL);
  if (dump_modules) {
    LLVMDumpModule(a);
    fputs("=============================================================\n",
          stderr);
  }
  if (hashmap_put(map_modules, name, a) != MAP_OK) {
    compiler_error_internal(
        "couldn't add module to list of modules upon compiling \"%s\"", name);
  }
  handle_llvm_error(
      LLVMOrcAddEagerlyCompiledIR(orcref, &modhdl, b, orcresolver, NULL));
}
void init_jit() {
  char *err;
  orcresolver = &orcresolverfun;

  map_modules = hashmap_new();

  dlhdl = dlopen("", RTLD_LAZY);
  bldr = LLVMCreateBuilder();

  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmParser();
  LLVMInitializeNativeAsmPrinter();
  LLVMLinkInMCJIT();

  LLVMTargetRef targetref;
  char *targettriple = LLVMGetDefaultTargetTriple();
  if (LLVMGetTargetFromTriple(targettriple, &targetref, &err)) {
    compiler_error_internal("error getting JIT target: %s", err);
  }
  if (!LLVMTargetHasJIT(targetref)) {
    compiler_error_internal("fatal error: target has no LLVM JIT");
  }
  tm = LLVMCreateTargetMachine(targetref, targettriple, "", "",
                               LLVMCodeGenLevelDefault, LLVMRelocDefault,
                               LLVMCodeModelJITDefault);
  if (!tm) {
    compiler_error_internal("error creating LLVM target machine");
  }
  LLVMDisposeMessage(targettriple);

  orcref = LLVMOrcCreateInstance(tm);
  if (!orcref) {
    compiler_error_internal("error creating LLVM ORC instance");
  }
}
int module_freer(any_t item, any_t value) {
  (void)item;
  LLVMDisposeModule((LLVMModuleRef)value);
  return MAP_OK;
}
void end_jit() {
  hashmap_iterate(map_modules, module_freer, NULL);
  hashmap_free(map_modules);
  LLVMOrcDisposeInstance(orcref);
  LLVMDisposeTargetMachine(tm);
  LLVMDisposeBuilder(bldr);
  dlclose(dlhdl);
}

void add_symbol_to_module(const char *name, LLVMModuleRef dest) {
  LLVMModuleRef m;
  if (hashmap_get(map_modules, name, (void **)&m) != MAP_OK) {
    compiler_error_internal("couldn't find symbol upon adding \"%s\"", name);
  }
  if (!m) {
    compiler_hint_internal("consider using \"copy-symbol-to-module\" instead");
    compiler_error_internal("attempted to add symbol \"%s\" to two modules",
                            name);
  }
  if (LLVMLinkModules2(dest, m)) {
    compiler_error_internal("error linking in symbol \"%s\"", name);
  }
  hashmap_put(map_modules, name, NULL);
}
void copy_symbol_to_module(const char *name, LLVMModuleRef dest) {
  LLVMModuleRef m, dupl;
  if (hashmap_get(map_modules, name, (void **)&m) != MAP_OK) {
    compiler_error_internal("couldn't find symbol upon adding \"%s\"", name);
  }
  if (!m) {
    compiler_hint_internal("use \"copy-symbol-to-module\" in the first call");
    compiler_error_internal("attempted to copy the already added symbol \"%s\"",
                            name);
  }
  dupl = LLVMCloneModule(m);
  if (LLVMLinkModules2(dest, dupl)) {
    compiler_error_internal("error linking in symbol \"%s\"", name);
  }
}
