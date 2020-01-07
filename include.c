#include "include.h"

#include "eval.h"
#include "fun.h"
#include "main.h"

#include <stdio.h>

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
struct rtv *lower_include(const char *filename) {
  struct val *list;
  list = read_file(filename);
  if (dump_lists) {
    print_list(list);
    printf("\n");
  }
  return progn(list);
}
