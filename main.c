#include "basictypes.h"
#include "eval.h"
#include "fun.h"
#include "global.h"
#include "include.h"
#include "jit.h"
#include "list.h"
#include "quote.h"
#include "structs.h"
#include "type.h"
#include "var.h"

#include <alloca.h>
#include <string.h>

int dump_modules, dump_lists;

int main(int argc, char **argv) {
  int i;
  int read_filename;
  struct val **files;
  dump_modules = 0;
  dump_lists = 0;
  read_filename = 0;
  enable_precompilation = 0;
  files = alloca(argc * sizeof(char *));
  for (i = 1; i < argc; ++i) {
    files[i] = NULL;
    if (strcmp(argv[i], "--dump-modules") == 0) {
      dump_modules = 1;
    } else if (strcmp(argv[i], "--dump-lists") == 0) {
      dump_lists = 1;
    } else if (strcmp(argv[i], "--enable-precompilation") == 0) {
      enable_precompilation = 1;
    } else if (strcmp(argv[i], "-") == 0) {
      files[i] = read_stdin();
    } else {
      files[i] = read_file(argv[i]);
    }
  }

  init_alloc();
  init_jit();
  init_types();
  init_fun();
  init_basictypes();
  init_global();
  init_var();
  init_quote();
  init_include();
  init_eval();
  init_structs();

  for (i = 1; i < argc; ++i) {
    if (files[i]) {
      lower_include_list(argv[i], files[i]);
      read_filename = 1;
    }
  }
  if (!read_filename) {
    compiler_error_internal("no filename specified");
  }

  end_structs();
  end_eval();
  end_include();
  end_quote();
  end_var();
  end_global();
  end_basictypes();
  end_fun();
  end_types();
  end_jit();
  end_alloc();
  return 0;
}
