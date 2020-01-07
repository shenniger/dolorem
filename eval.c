#include "eval.h"

#include "basictypes.h"
#include "fun.h"
#include "global.h"
#include "jit.h"
#include <assert.h>
#include <stddef.h>

void init_eval() {
  lower_macroproto("progn");
  lower_macroproto("scope");
}
void end_eval() {}

struct rtv *eval(struct val *e) {
  switch (e->T) {
  case tyCons: {
    struct val *name, *args;
    struct fun *f;
    name = car(e);
    args = cdr(e);
    if (name->T != tyIdent) {
      compiler_error(name, "expected function name");
    }
    f = lookup_fun(name->V.S);
    if (!f) {
      compiler_error(e, "unknown function \"%s\"", name->V.S);
    }
    if (f->type.flags & ffTypeMacro) {
      compiler_error(e, "attempt to directly call type macro");
    }
    if (f->type.flags & ffMacro) {
      return call_fun_macro(f->name, args);
    } else {
      return funcall(f, args);
    }
  }
  case tyInt:
    return make_rtv(
        LLVMConstInt(LLVMInt64Type(), e->V.I, 0),
        make_rtt(LLVMInt64Type(), basictypes_integer, (void *)(0x100 & 64), 0));
  case tyFloat:
    assert(!"not impl"); /* TODO */
  case tyIdent: {
    struct funvar *f;
    struct global *g;
    if (curfn && ((f = lookup_in_fun_scope(curfn, e->V.S)))) {
      struct rtv n;
      n = f->v;
      n.v = LLVMBuildLoad(bldr, f->v.v, e->V.S);
      return copy_rtv(n);
    }
    if (hashmap_get(map_globals, e->V.S, (void **)&g) == MAP_OK) {
      struct rtv n;
      LLVMValueRef v;
      n.t = g->type->t;
      v = LLVMGetNamedGlobal(mod, g->name);
      if (!v) {
        v = LLVMAddGlobal(mod, g->type->l, g->name);
      }
      n.v = LLVMBuildLoad(bldr, v, e->V.S);
      return copy_rtv(n);
    }
    compiler_error(e, "unknown identifier \"%s\"", e->V.S);
  }
  case tyString:
    return make_rtv(LLVMBuildGlobalStringPtr(bldr, e->V.S, "strliteral"),
                    lower_pointer_type(lower_integer_type(8, 0)));
  default:
    assert(!"unknown type");
  }
}

struct rtv *progn(struct val *e) {
  struct rtv *v;
  if (is_nil(e)) {
    return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
  }
  do {
    v = eval(car(e));
  } while (!is_nil(e = cdr(e)));
  return v;
}

struct rtv *scope(struct val *e) {
  struct funscope *s;
  struct rtv *r;
  if (!curfn) {
    return progn(e);
  }
  s = get_mem(sizeof(struct funscope));
  s->parent = curfn->scope;
  curfn->scope = s;
  r = progn(e);
  curfn->scope = curfn->scope->parent;
  return r;
}

struct rtv *funcall(struct fun *a, struct val *args) {
  LLVMValueRef *v;
  LLVMValueRef fun;
  struct rtv r;
  long i;
  if (count_len(args) != a->type.nparms) {
    compiler_error(args, "expected %i arguments, found %i", a->type.nparms,
                   count_len(args));
  }
  if (a->type.nparms) {
    v = get_mem(sizeof(LLVMValueRef *) * a->type.nparms);
    i = 0;
    do {
      struct rtv *c;
      struct rtv *from;
      from = eval(car(args));
      c = convert_type(from, &a->type.parms[i].t, 0);
      if (!c) {
        compiler_error(car(args), "couldn't convert type \"%s\" to \"%s\"",
                       print_type(&from->t), print_type(&a->type.parms[i].t.t));
      }
      v[i] = c->v;
      ++i;
    } while (!is_nil(args = cdr(args)));
  } else {
    v = NULL;
  }
  fun = LLVMGetNamedFunction(mod, a->name);
  if (!fun) {
    fun = LLVMAddFunction(mod, a->name, a->type.funtype);
    fun_set_proper_parm_names(a, fun);
  }
  r.v = LLVMBuildCall2(bldr, a->type.funtype, fun, v, a->type.nparms,
                       a->type.ret.t.info ? print_to_mem("funcall_%s", a->name)
                                          : "");
  r.t = a->type.ret.t;
  return copy_rtv(r);
}

struct rtv *call_fun_macro(const char *name, struct val *e) {
  return ((struct rtv * (*)(struct val * e))resolve_sym(name))(e);
}
struct rtt *call_type_macro(const char *name, struct val *e, void *prop) {
  return ((struct rtt * (*)(struct val * e, void *prop))resolve_sym(name))(
      e, prop);
}
struct rtv *call_type_converter(const char *name, struct rtv *value,
                                struct rtt *to, int is_explicit_cast) {
  return ((struct rtv * (*)(struct rtv * value, struct rtt * to,
                            int is_explicit_cast))resolve_sym(name))(
      value, to, is_explicit_cast);
}
