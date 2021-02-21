#include "include.h"

#include "eval.h"
#include "fun.h"
#include "main.h"

#include <stdio.h>

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

LLVMModuleRef precompiled_module;
int enable_precompilation;

void init_include() { lower_macroproto("include"); }
void end_include() {}
struct rtv *include(struct val *l) {
  struct val *name;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"include\"");
  }
  if (name->T != tyString) {
    compiler_error(name, "expected string");
  }
  return lower_include(name->V.S);
}
struct rtv *lower_include_list(const char *filename, struct val *list) {
  struct rtv *r;
  LLVMModuleRef pcmod_before;
  const char *so_path;
  if (dump_lists) {
    print_list(list);
    printf("\n");
  }
  pcmod_before = precompiled_module;
  {
    struct stat src, so;
    if (enable_precompilation && *filename != '<') {
      so_path = print_to_mem("./%s_pc.so", filename);
      stat(filename, &src);
    }
    if (enable_precompilation && *filename != '<' && stat(so_path, &so) == 0 &&
        src.st_mtime < so.st_mtime) {
      precompiled_module = NULL;
      if (!dlopen(so_path, RTLD_LAZY | RTLD_GLOBAL)) {
        compiler_error_internal("failure loading precompiled file \"%s\": %s",
                                so_path, dlerror());
      }
    } else {
      precompiled_module = LLVMModuleCreateWithName(filename);
    }
  }
  r = progn(list);
  {
    if (precompiled_module && enable_precompilation && *filename != '<') {
      const char *bc_path;
      bc_path = print_to_mem("%s_pc.bc", filename);
      LLVMWriteBitcodeToFile(precompiled_module, bc_path);
      fprintf(stderr, "Precompiling %s…\n", filename);
      system(
          print_to_mem("clang %s -o %s -fpic -shared -O3", bc_path, so_path));
      fprintf(stderr, "Done. %s written.\n", so_path);
    }
    precompiled_module = pcmod_before;
  }
  return r;
}
struct rtv *lower_include(const char *filename) {
  return lower_include_list(filename, read_file(filename));
}
