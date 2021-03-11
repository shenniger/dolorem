#include "jit.h"

#include "fun.h"
#include "global.h"
#include "list.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

#include "llvmext.h"

LLVMModuleRef mod, typemod;
LLVMOrcJITStackRef orcref;
LLVMOrcModuleHandle modhdl;
LLVMBuilderRef bldr;
LLVMTargetMachineRef tm;
void *dlhdl;
LLVMOrcSymbolResolverFn orcresolver;
map_t map_modules;
int dump_modules, dump_lists;

void handle_llvm_error(LLVMErrorRef e) {
  if (e) {
    char *m;
    m = LLVMGetErrorMessage(e);
    compiler_error_internal("LLVM error: %s", m);
  }
}

uint64_t resolve_sym(struct fun *a) {
  if (!a->ptr) {
    LLVMOrcTargetAddress r;
    handle_llvm_error(LLVMOrcGetSymbolAddress(orcref, &r, a->name));
    if (!r) {
      if (!((r = (uint64_t)(uintptr_t)dlsym(dlhdl, a->name)))) {
        compiler_error_internal(
            "couldn't resolve function symbol in JIT: \"%s\"", a->name);
      }
    }
    a->ptr = (uint64_t)(uintptr_t)r;
  }
  return a->ptr;
}

static inline uint64_t orcresolverfun(const char *name, void *ctx) {
  struct fun *f;
  (void)ctx;
  f = lookup_fun(name);
  if (!f) {
    struct global *g;
    g = lookup_global(name);
    if (!g) {
      compiler_error_internal("orc attempted to lookup unknown symbol: \"%s\"",
                              name);
    }

    if (!g->ptr) {
      LLVMOrcTargetAddress r;
      handle_llvm_error(LLVMOrcGetSymbolAddress(orcref, &r, g->name));
      if (!r) {
        if (!((r = (uint64_t)(uintptr_t)dlsym(dlhdl, g->name)))) {
          compiler_error_internal(
              "couldn't resolve function symbol in JIT: \"%s\"", g->name);
        }
      }
      g->ptr = (uint64_t)(uintptr_t)r;
    }
    return g->ptr;
  }
  return resolve_sym(f);
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

  typemod = LLVMModuleCreateWithName("test");
}
int module_freer(any_t item, any_t value) {
  (void)item;
  LLVMDisposeModule((LLVMModuleRef)value);
  return MAP_OK;
}
void end_jit() {
  hashmap_iterate(map_modules, module_freer, NULL);
  hashmap_free(map_modules);
  LLVMDisposeModule(typemod);
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
