#include "var.h"

#include "eval.h"
#include "fun.h"
#include "hashmap.h"
#include "list.h"
#include "type.h"

void init_var() { lower_macroproto("assign"); }
void end_var() {}

/* adds a variable to the current scope */
struct rtv *add_variable(const char *name, struct rtt *type) {
  struct funvar *n;
  n = get_mem(sizeof(struct funvar));
  n->last = curfn->scope->vars;
  curfn->scope->vars = n;
  n->hashed_name = hash_of_string(name);
  n->name = name;
  n->v = *make_rtv(LLVMBuildAlloca(bldr, type->l, name), type);
  n->t = *type;
  return &n->v;
}

void lower_assign(const char *name, struct rtv *rhs) {
  struct funvar *var;
  struct rtv *conv;
  var = lookup_in_fun_scope(curfn, name);
  if (!var) {
    compiler_error_internal("unknown variable: \"%s\"", name);
  }
  conv = convert_type(rhs, &var->t, 0);
  if (!conv) {
    compiler_error_internal("couldn't convert \"%s\" to \"%s\" implicitly",
                            print_type(&rhs->t), print_type(&var->t.t));
  }
  LLVMBuildStore(bldr, conv->v, var->v.v);
}

struct rtv *assign(struct val *e) {
  const char *lhs;
  struct rtv *rhs;
  lhs = expect_ident(car(e)); /* TODO: this is too restrictive */
  rhs = eval(car(cdr(e)));
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(e, "excess elements in \"assign\"");
  }
  lower_assign(lhs, rhs);
  return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
}
