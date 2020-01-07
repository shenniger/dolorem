#include "basictypes.h"
#include "eval.h"
#include "fun.h"
#include "global.h"
#include "include.h"
#include "jit.h"
#include "list.h"
#include "quote.h"
#include "type.h"
#include "var.h"

#include <string.h>

int dump_modules, dump_lists;

int main(int argc, char **argv) {
  int i;
  int read_filename;
  dump_modules = 0;
  dump_lists = 0;
  read_filename = 0;
  for (i = 1; i < argc; ++i) {
    if (*argv[i] == '-') {
      if (strcmp(argv[i], "--dump-modules") == 0) {
        dump_modules = 1;
      } else if (strcmp(argv[i], "--dump-lists") == 0) {
        dump_lists = 1;
      }
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
  init_include(dump_lists);
  init_eval();

  for (i = 1; i < argc; ++i) {
    if (*argv[i] != '-') {
      lower_include(argv[i]);
      read_filename = 1;
    }
  }
  if (!read_filename) {
    compiler_error_internal("no filename specified");
  }

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
