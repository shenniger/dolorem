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
  lower_macroproto("call_funptr");
  lower_macroproto("funptr_to");
  lower_macroproto("gdb_break_here");
}
void end_eval() {}

struct rtv *gdb_break_here(struct val *e) {
  /* for gdb */
  (void)e;
  return &null_rtv;
}

struct rtv *make_int_const(long i) {
  return make_rtv(
      LLVMConstInt(LLVMInt64Type(), i, 0),
      make_rtt(LLVMInt64Type(), basictypes_integer, (void *)(0x100 | 64), 0),
      vfR);
}
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
      return call_fun_macro(f, args);
    } else {
      return funcall(f, args);
    }
  }
  case tyInt:
    return make_int_const(e->V.I);
  case tyFloat:
    return make_rtv(
        LLVMConstReal(LLVMDoubleType(), e->V.F),
        make_rtt(LLVMDoubleType(), basictypes_float, (void *)0x2, 0), vfR);
  case tyIdent: {
    struct funvar *f;
    struct global *g;
    if (curfn && ((f = lookup_in_fun_scope(curfn, e->V.S)))) {
      struct rtv n;
      n = f->v;
      n.v = f->v.v;
      n.t.value_flags = vfL;
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
      n.v = v;
      n.t.value_flags = vfL;
      return copy_rtv(n);
    }
    compiler_error(e, "unknown identifier \"%s\"", e->V.S);
  }
  case tyString:
    return make_rtv(LLVMBuildGlobalStringPtr(bldr, e->V.S, "strliteral"),
                    lower_pointer_type(lower_integer_type(8, 0)), vfR);
  default:
    assert(!"unknown type");
  }
}

struct rtv *progn(struct val *e) {
  struct rtv *v;
  if (is_nil(e)) {
    return &null_rtv;
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

struct rtv *lower_funcall(LLVMValueRef fun, struct funtypeprop funtype,
                          struct val *args) {
  LLVMValueRef *v;
  struct rtv r;
  long i, count;
  count = count_len(args);
  if (count != funtype.nparms && !(funtype.flags & ffVaArgs)) {
    compiler_error(args, "expected %i arguments, found %i", funtype.nparms,
                   count_len(args));
  }
  if ((funtype.flags & ffVaArgs) && count < funtype.nparms) {
    compiler_error(args, "expected at least %i arguments, found %i",
                   funtype.nparms, count_len(args));
  }
  if (funtype.nparms) {
    v = get_mem(sizeof(LLVMValueRef *) * funtype.nparms);
    i = 0;
    do {
      struct rtv *c;
      struct rtv *from;
      from = prepare_read(eval(car(args)));
      if (i >= funtype.nparms) {
        c = from; /* TODO: handle VA-ARGS properly according to
                     the calling convention */
      } else {
        c = convert_type(from, &funtype.parms[i].t, 0);
        if (!c) {
          compiler_error(car(args), "couldn't convert type \"%s\" to \"%s\"",
                         print_type(&from->t),
                         print_type(&funtype.parms[i].t.t));
        }
      }
      v[i] = c->v;
      ++i;
    } while (!is_nil(args = cdr(args)));
  } else {
    v = NULL;
  }
  r.v = LLVMBuildCall2(bldr, funtype.funtype, fun, v, count,
                       funtype.ret.t.info
                           ? print_to_mem("funcall_%s", LLVMGetValueName(fun))
                           : "");
  r.t = funtype.ret.t;
  r.t.value_flags = vfR;
  return copy_rtv(r);
}
struct rtv *funcall(struct fun *a, struct val *args) {
  LLVMValueRef fun;
  fun = LLVMGetNamedFunction(mod, a->name);
  if (!fun) {
    fun = LLVMAddFunction(mod, a->name, a->type.funtype);
    fun_set_proper_parm_names(a, fun);
  }
  return lower_funcall(fun, a->type, args);
}

struct rtv *call_fun_macro(struct fun *name, struct val *e) {
  return ((struct rtv * (*)(struct val * e)) resolve_sym(name))(e);
}
struct rtt *call_type_macro(struct fun *name, struct val *e, void *prop) {
  return ((struct rtt * (*)(struct val * e, void *prop))
              resolve_sym(name))(e, prop);
}
struct rtv *call_type_converter(struct fun *name, struct rtv *value,
                                struct rtt *to, int is_explicit_cast) {
  return ((struct rtv *
           (*)(struct rtv * value, struct rtt * to, int is_explicit_cast))
              resolve_sym(name))(value, to, is_explicit_cast);
}

struct rtv *funptr_to(struct val *e) {
  struct val *name;
  LLVMValueRef fun;
  struct fun *f;
  name = car(e);
  if (is_nil(name) || !is_nil(cdr(e))) {
    compiler_error(e, "expected exactly one argument to \"funptr-to\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected function name");
  }
  f = lookup_fun(name->V.S);
  if (!f) {
    compiler_error(e, "unknown function \"%s\"", name->V.S);
  }
  fun = LLVMGetNamedFunction(mod, name->V.S);
  if (!fun) {
    fun = LLVMAddFunction(mod, f->name, f->type.funtype);
    fun_set_proper_parm_names(f, fun);
  }
  return make_rtv(fun, make_rtt(f->type.funtype, funptr, &f->type, 0), vfR);
}
struct rtv *call_funptr(struct val *e) {
  struct rtv *fptr;
  struct val *args;
  fptr = prepare_read(eval(car(e)));
  args = cdr(e);
  if (fptr->t.info != funptr) {
    compiler_error(car(e), "expected function pointer type, found: %s",
                   print_type(&fptr->t));
  }
  return lower_funcall(fptr->v, *fptr->t.prop.fun, args);
}
