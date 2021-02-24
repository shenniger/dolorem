#include "fun.h"

#include "basictypes.h"
#include "eval.h"
#include "include.h"
#include "jit.h"
#include "list.h"

#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

map_t map_funs;
map_t map_aliases;
struct fun *curfn;
LLVMValueRef curllvmfn;

struct typealias *rt_val_type, *rt_rtt_type, *rt_rtv_type;
struct typeinf *funptr;

static const char *print_funptr(struct type *t);

void init_fun() {
  map_funs = hashmap_new();
  assert(map_funs);
  map_aliases = hashmap_new();
  assert(map_aliases);

  funptr = register_type_class("funptr", print_funptr);
}
void end_fun() { hashmap_free(map_funs); }

static const char *print_funptr(struct type *t) {
  const char *s;
  long i;
  s = "funptr (";
  for (i = 0; i < t->prop.fun->nparms; ++i) {
    s = print_to_mem(i ? "%s (%s %s)" : "%s(%s %s)", s,
                     print_type(&t->prop.fun->parms[i].t.t),
                     t->prop.fun->parms[i].name);
  }
  s = print_to_mem("%s) %s", s, print_type(&t->prop.fun->ret.t));
  return s;
}

LLVMTypeRef fun_type_to_llvm(struct funtypeprop a) {
  LLVMTypeRef *parms = alloca(a.nparms * sizeof(LLVMTypeRef));
  for (long i = 0; i < a.nparms; ++i) {
    parms[i] = a.parms[i].t.l;
  }
  return LLVMFunctionType(a.ret.l, parms, a.nparms, a.flags & ffVaArgs);
}
static struct funparm *parse_parms(struct val l, long *nparms, long *flags) {
  struct funparm *r;
  long i;
  *nparms = count_len(&l);
  r = get_mem(sizeof(struct funparm) * *nparms);
  if (is_nil(car(&l))) {
    return r;
  }
  i = 0;
  do {
    struct val *e, *name, *type;
    e = car(&l);
    if (e->T == tyIdent) {
      if (strcmp(e->V.S, "...") == 0) {
        l = *cdr(&l);
        if (!is_nil(&l)) {
          compiler_error(e, "vaargs not last in argument list");
        }
        *flags |= ffVaArgs;
        --*nparms;
        break;
      }
    }
    if (e->T != tyCons) {
      compiler_error(e, "expected element of list in parameter list");
    }
    type = car(e);
    name = car(cdr(e));
    if (name->T != tyIdent) {
      compiler_error(e, "expected parameter name in parameter list");
    }
    r[i].name = name->V.S;
    r[i].t = *eval_type(type);
    ++i;
    l = *cdr(&l);
  } while (!is_nil(&l));
  return r;
}

void fun_set_proper_parm_names(struct fun *f, LLVMValueRef fun) {
  long i;
  for (i = 0; i < f->type.nparms; ++i) {
    LLVMSetValueName(LLVMGetParam(fun, i), f->type.parms[i].name);
  }
}

struct fun *lookup_fun(const char *name) {
  struct fun *r;
  if (hashmap_get(map_funs, name, (void **)&r) != MAP_OK) {
    return NULL;
  }
  return r;
}

struct rtv *funproto(struct val *l) {
  struct val *name, *parms, *ret;
  struct fun *f;
  name = car(l);
  if (name->T != tyIdent) {
    compiler_error(name, "expected name in function prototype");
  }
  parms = car(cdr(l));
  if (parms->T != tyCons) {
    compiler_error(parms, "expected list of parameters in function prototype");
  }
  ret = car(cdr(cdr(l)));
  if (!is_nil(cdr(cdr(cdr(l))))) {
    compiler_error(l, "more arguments than expected in function prototype");
  }

  f = get_mem(sizeof(struct fun));
  f->name = name->V.S;
  f->type.ret = *eval_type(ret);
  f->type.parms = parse_parms(*parms, &f->type.nparms, &f->type.flags);
  if (lookup_fun(name->V.S)) {
    compiler_error(l, "function already exists: \"%s\"", name);
  }
  if (hashmap_put(map_funs, name->V.S, f) != MAP_OK) {
    compiler_error(l, "error inserting function into hashmap: \"%s\"", name);
  }
  f->type.funtype = fun_type_to_llvm(f->type);
  return &null_rtv;
}
struct rtv *defun(struct val *l) {
  struct val *name, *parms, *ret, *body;
  struct fun *f;
  name = car(l);
  if (name->T != tyIdent) {
    compiler_error(name, "expected name in function");
  }
  parms = car(cdr(l));
  if (parms->T != tyCons) {
    compiler_error(parms, "expected list of parameters in function definition");
  }
  ret = car(cdr(cdr(l)));
  if (!is_nil(cdr(cdr(cdr(cdr(l)))))) {
    compiler_error(l, "more arguments than expected in function definition");
  }
  body = car(cdr(cdr(cdr(l))));
  if (is_nil(body)) {
    compiler_error(l, "expected function body");
  }

  f = get_mem(sizeof(struct fun));
  f->name = name->V.S;
  f->type.ret = *eval_type(ret);
  f->type.parms = parse_parms(*parms, &f->type.nparms, &f->type.flags);
  f->type.flags = ffImplemented;
  if (lookup_fun(name->V.S)) {
    compiler_error(l, "function already exists: \"%s\"", name);
  }
  if (hashmap_put(map_funs, name->V.S, f) != MAP_OK) {
    compiler_error(l, "error inserting function into hashmap: \"%s\"", name);
  }

  f->type.funtype = fun_type_to_llvm(f->type);
  funbody(f, body);
  return &null_rtv;
}

struct fun *lower_macroproto(const char *name) {
  struct fun *a;
  a = get_mem(sizeof(struct fun));
  a->type.flags = ffMacro;
  a->type.nparms = 1;
  a->type.parms = get_mem(sizeof(struct funparm) * 1);
  a->type.parms[0].name = "args";
  a->type.parms[0].t = *lower_alias_type(rt_val_type);
  a->type.ret = *lower_alias_type(rt_rtv_type);
  a->name = name;
  a->type.funtype = fun_type_to_llvm(a->type);
  if (hashmap_put(map_funs, name, a) != MAP_OK) {
    compiler_error_internal("error inserting function into hashmap: \"%s\"",
                            name);
  }
  return a;
}
struct fun *lower_typemacroproto(const char *name) {
  struct fun *a;
  a = get_mem(sizeof(struct fun));
  a->type.flags = ffTypeMacro;
  a->type.nparms = 2;
  a->type.parms = get_mem(sizeof(struct funparm) * 2);
  a->type.parms[0].name = "args";
  a->type.parms[0].t = *lower_alias_type(rt_val_type);
  a->type.parms[1].name = "prop";
  a->type.parms[1].t = *pointer_type(NULL, NULL);
  a->type.ret = *lower_alias_type(rt_rtt_type);
  a->name = name;
  a->type.funtype = fun_type_to_llvm(a->type);
  if (hashmap_put(map_funs, name, a) != MAP_OK) {
    compiler_error_internal("error inserting function into hashmap: \"%s\"",
                            name);
  }
  return a;
}
struct fun *lower_typeconverterproto(const char *name) {
  struct fun *a;
  a = get_mem(sizeof(struct fun));
  a->type.flags = ffTypeConverter;
  a->type.nparms = 3;
  a->type.parms = get_mem(sizeof(struct funparm) * 3);
  a->type.parms[0].name = "v";
  a->type.parms[0].t = *lower_alias_type(rt_rtv_type);
  a->type.parms[1].name = "to";
  a->type.parms[1].t = *lower_alias_type(rt_rtt_type);
  a->type.parms[2].name = "is_explicit";
  a->type.parms[2].t = *lower_integer_type(32, 0);
  a->type.ret = *lower_alias_type(rt_rtv_type);
  a->name = name;
  a->type.funtype = fun_type_to_llvm(a->type);
  if (hashmap_put(map_funs, name, a) != MAP_OK) {
    compiler_error_internal("error inserting function into hashmap: \"%s\"",
                            name);
  }
  return a;
}

struct funvar *lookup_in_fun_scope(struct fun *f, const char *name) {
  unsigned int h;
  struct funscope *scope;
  h = hash_of_string(name);
  for (scope = f->scope; scope; scope = scope->parent) {
    struct funvar *v;
    for (v = scope->vars; v; v = v->last) {
      if (v->hashed_name == h && strcmp(name, v->name) == 0) {
        /* match */
        return v;
      }
    }
  }
  return NULL;
}

void funbody(struct fun *f, struct val *body) {
  LLVMBasicBlockRef entry;
  LLVMValueRef fun;
  struct fun *prev;
  LLVMValueRef prev2;
  struct rtv ret;
  long i;
  LLVMValueRef *params;

  if (!precompiled_module) {
    return;
  }

  begin_new_function();
  fun = LLVMAddFunction(mod, f->name, f->type.funtype);
  fun_set_proper_parm_names(f, fun);

  entry = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(bldr, entry);

  /* build scope */
  f->scope = get_mem(sizeof(struct funscope));
  params = alloca(f->type.nparms * sizeof(LLVMValueRef));
  LLVMGetParams(fun, params);
  for (i = 0; i < f->type.nparms; ++i) {
    struct funvar *n;
    LLVMValueRef p;
    p = LLVMBuildAlloca(bldr, f->type.parms[i].t.l, f->type.parms[i].name);
    LLVMBuildStore(bldr, params[i], p);
    n = get_mem(sizeof(struct funvar));
    n->hashed_name = hash_of_string(f->type.parms[i].name);
    n->name = f->type.parms[i].name;
    n->last = f->scope->vars;
    n->v.v = p;
    n->v.t = f->type.parms[i].t.t;
    n->t = f->type.parms[i].t;
    f->scope->vars = n;
  }

  prev = curfn;
  prev2 = curllvmfn;
  curfn = f;
  curllvmfn = fun;
  ret = *prepare_read(eval(body));
  if (f->type.ret.t.info) {
    struct rtv *c;
    c = convert_type(&ret, &f->type.ret, 0);
    if (!c) {
      compiler_error(
          body, "couldn't convert result of type \"%s\" to return type \"%s\"",
          print_type(&ret.t), print_type(&f->type.ret.t));
    }
    ret = *c;
    LLVMBuildRet(bldr, ret.v);
  } else {
    LLVMBuildRetVoid(bldr);
  }
  curfn = prev;
  curllvmfn = prev2;
  end_function(f->name);
  add_symbol_to_module(f->name, precompiled_module);
}

struct rtv *macroproto(struct val *l) {
  struct val *name;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"macroproto\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_macroproto(name->V.S);
  return &null_rtv;
}
struct rtv *defmacro(struct val *l) {
  struct val *name, *body;
  name = car(l);
  body = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"defmacro\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  funbody(lower_macroproto(name->V.S), body);
  return &null_rtv;
}
struct rtv *typemacroproto(struct val *l) {
  struct val *name;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"typemacroproto\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_typemacroproto(name->V.S);
  return &null_rtv;
}
struct rtv *deftypemacro(struct val *l) {
  struct val *name, *body;
  name = car(l);
  body = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"deftypemacro\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  funbody(lower_typemacroproto(name->V.S), body);
  return &null_rtv;
}

struct rtv *typeconverterproto(struct val *l) {
  struct val *name;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"typeconverterproto\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_typeconverterproto(name->V.S);
  return &null_rtv;
}
struct rtv *deftypeconverter(struct val *l) {
  struct val *name, *body;
  name = car(l);
  body = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"deftypeconverter\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  funbody(lower_typeconverterproto(name->V.S), body);
  return &null_rtv;
}

struct rtt *funptr_type(struct val *l, void *prop) {
  struct val *parms, *ret;
  struct funtypeprop *f;
  (void)prop;
  parms = car(l);
  if (parms->T != tyCons) {
    compiler_error(parms,
                   "expected list of parameters in function pointer type");
  }
  ret = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(l, "more arguments than expected in function pointer type");
  }

  f = get_mem(sizeof(struct funtypeprop));
  f->ret = *eval_type(ret);
  f->parms = parse_parms(*parms, &f->nparms, &f->flags);
  f->funtype = fun_type_to_llvm(*f);
  return make_rtt(LLVMPointerType(f->funtype, 0), funptr, f, 0);
}
