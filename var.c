#include "var.h"

#include "eval.h"
#include "fun.h"
#include "hashmap.h"
#include "list.h"
#include "type.h"

#include "global.h"
#include "hashmap.h"

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
  n->v = *make_rtv(LLVMBuildAlloca(bldr, type->l, name), type, vfL);
  n->t = *type;
  return copy_rtv(n->v);
}

void lower_assign(struct rtv *lhs, struct rtv *rhs, struct val *e) {
  struct rtv *conv;
  if (!(lhs->t.value_flags & vfL)) {
    compiler_error(e, "attempt to assign to an r-value");
  }
  rhs = prepare_read(rhs);
  conv = convert_type(
      rhs, make_rtt_from_type(LLVMGetElementType(LLVMTypeOf(lhs->v)), lhs->t),
      0);
  if (!conv) {
    compiler_error(e, "couldn't convert \"%s\" to \"%s\" implicitly",
                   print_type(&rhs->t), print_type(&lhs->t));
  }
  LLVMBuildStore(bldr, conv->v, lhs->v);
}

struct rtv *assign(struct val *e) {
  struct rtv *lhs;
  struct rtv *rhs;
  lhs = eval(car(e));
  rhs = eval(car(cdr(e)));
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(e, "excess elements in \"assign\"");
  }
  lower_assign(lhs, rhs, e);
  return &null_rtv;
}
