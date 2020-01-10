#include "global.h"

#include "fun.h"
#include "include.h"

map_t map_globals;

void init_global() {
  map_globals = hashmap_new();
  lower_macroproto("defglobal");
  lower_macroproto("external_global");
}
void end_global() {}

void lower_defglobal(const char *name, struct rtt *type, int isextern) {
  struct global *g;
  void *dummy;
  g = get_mem(sizeof(struct global));
  g->name = name;
  g->type = type;
  if (hashmap_get(map_globals, name, &dummy) == MAP_OK) {
    compiler_error_internal("global already defined: \"%s\"", name);
  }
  if (hashmap_put(map_globals, name, g) != MAP_OK) {
    compiler_error_internal("error defining global \"%s\"", name);
  }
  if (!isextern && precompiled_module) {
    add_global_symbol(name, type->l);
    add_symbol_to_module(name, precompiled_module);
  }
}
struct rtv *defglobal(struct val *l) {
  struct val *name, *type;
  type = car(l);
  name = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"defglobal\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_defglobal(name->V.S, eval_type(type), 0);
  return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
}
struct rtv *external_global(struct val *l) {
  struct val *name, *type;
  type = car(l);
  name = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"external_global\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_defglobal(name->V.S, eval_type(type), 1);
  return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
}
