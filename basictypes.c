#include "basictypes.h"

#include "eval.h"
#include "fun.h"
#include "jit.h"

#include <assert.h>
#include <stddef.h>

struct typeinf *basictypes_integer, *basictypes_alias, *basictypes_pointer,
    *basictypes_float;

int is_int(struct rtt *a);
int is_float(struct rtt *a);

static const char *print_alias_type(struct type *t) {
  return t->prop.alias->name;
}
static const char *print_integer_type(struct type *t) {
  uint64_t a;
  const char *size;
  a = (uint64_t)t->prop.any;
  switch (a & 0xff) {
  case 1:
    return "bool";
  case 8:
    size = "8";
    break;
  case 16:
    size = "16";
    break;
  case 32:
    size = "32";
    break;
  case 64:
    size = "64";
    break;
  default:
    assert(0);
  }
  if (a & 0x100) {
    return print_to_mem("i%s", size);
  } else {
    return print_to_mem("u%s", size);
  }
}
static const char *print_float_type(struct type *t) {
  switch (t->prop.num) {
  case 1:
    return "float";
  case 2:
    return "double";
  default:
    assert(0);
  }
}
static const char *print_pointer_type(struct type *t) {
  return print_to_mem("ptr %s", print_type(t->prop.type));
}

struct rtv *convert_alias_unwrap(struct rtv *v, struct rtt *to,
                                 int is_explicit) {
  struct rtv *newv;
  struct rtt *newto;
  int foundalias;
  (void)is_explicit;
  foundalias = 0;
  newv = v;
  newto = to;
  if (v->t.info == basictypes_alias && v->t.prop.alias->l) {
    newv = copy_rtv(*v);
    newv->t = newv->t.prop.alias->l->t;
    foundalias = 1;
  }
  if (to->t.info == basictypes_alias && to->t.prop.alias->l) {
    newto = to->t.prop.alias->l;
    foundalias = 1;
  }
  if (foundalias) {
    return convert_type(newv, newto, 0);
  } else {
    return NULL;
  }
}

struct rtv *convert_pointer_types(struct rtv *v, struct rtt *to,
                                  int is_explicit) {
  if (v->t.info == basictypes_pointer && to->t.info == basictypes_pointer) {
    if (!is_explicit) {
      /* TODO: This rule is too lenient and doesn't handle volatility and
       * constness correctly. */
      if (!v->t.prop.type || !to->t.prop.type) {
        return v;
      }
      if (v->t.prop.type->prop.any == to->t.prop.type->prop.any) {
        return v;
      }
      return NULL;
    }
    struct rtv *c;
    c = copy_rtv(*v);
    c->t.prop.type = to->t.prop.type;
    return c;
  }
  return NULL;
}

struct rtv *convert_numbers(struct rtv *v, struct rtt *to, int is_explicit) {
  if (v->t.info == basictypes_integer && to->t.info == basictypes_integer) {
    /* TODO: pay attention to signedness */
    if (!is_explicit && ((v->t.prop.num & 0xff) > (to->t.prop.num & 0xff))) {
      return NULL;
    }
    struct rtv *c;
    c = copy_rtv(*v);
    c->t.prop.num = to->t.prop.num;
    c->v = LLVMBuildIntCast(bldr, v->v, to->l, "intcast");
    return c;
  }
  if (v->t.info == basictypes_float && to->t.info == basictypes_float) {
    if (!is_explicit && (v->t.prop.num < to->t.prop.num)) {
      return NULL;
    }
    struct rtv *c;
    c = copy_rtv(*v);
    c->t.prop.num = to->t.prop.num;
    c->v = LLVMBuildFPCast(bldr, v->v, to->l, "floatcast");
    return c;
  }
  if (v->t.info == basictypes_integer && to->t.info == basictypes_float) {
    if (v->t.prop.num & 0x100) {
      return make_rtv(LLVMBuildSIToFP(bldr, v->v, to->l, "sitofp"), to);
    } else {
      return make_rtv(LLVMBuildUIToFP(bldr, v->v, to->l, "uitofp"), to);
    }
  }
  if (v->t.info == basictypes_float && to->t.info == basictypes_integer &&
      is_explicit) {
    if (to->t.prop.num & 0x100) {
      return make_rtv(LLVMBuildFPToSI(bldr, v->v, to->l, "fptosi"), to);
    } else {
      return make_rtv(LLVMBuildFPToUI(bldr, v->v, to->l, "fptoui"), to);
    }
  }
  return NULL;
}

void init_basictypes() {
  basictypes_alias = register_type_class("alias", print_alias_type);
  basictypes_integer = register_type_class("integer", print_integer_type);
  basictypes_pointer = register_type_class("pointer", print_pointer_type);
  basictypes_float = register_type_class("float", print_float_type);

  lower_macroproto("create_alias");
  lower_macroproto("ptr_deref");
  lower_macroproto("ptr_to");

  lower_typemacroproto("void_type");
  lower_typemacroproto("alias_type");
  lower_typemacroproto("integer_type");
  lower_typemacroproto("pointer_type");
  register_type("void", "void_type", NULL);
  register_type("bool", "integer_type", (void *)(1 << 8 | 1));
  register_type("char", "integer_type", (void *)(0 << 8 | 8));
  register_type("i8", "integer_type", (void *)(1 << 8 | 8));
  register_type("i16", "integer_type", (void *)(1 << 8 | 16));
  register_type("i32", "integer_type", (void *)(1 << 8 | 32));
  register_type("i64", "integer_type", (void *)(1 << 8 | 64));
  register_type("u8", "integer_type", (void *)(0 << 8 | 8));
  register_type("u16", "integer_type", (void *)(0 << 8 | 16));
  register_type("u32", "integer_type", (void *)(0 << 8 | 32));
  register_type("u64", "integer_type", (void *)(0 << 8 | 64));
  register_type("ptr", "pointer_type", NULL);
  register_type("float", "float_type", (void *)1);
  register_type("double", "float_type", (void *)2);

  lower_register_type_converter("convert_pointer_types");
  lower_register_type_converter("convert_alias_unwrap");
  lower_register_type_converter("convert_numbers");
}

void end_basictypes() {}
struct typealias *lower_create_opaque(LLVMTypeRef t, const char *name) {
  struct typealias *a;
  a = get_mem(sizeof(struct typealias));
  a->name = name;
  a->t = t;
  a->l = NULL;
  register_type(name, "alias_type", a);
  return a;
}
struct typealias *lower_create_alias(struct rtt *l, const char *name) {
  struct typealias *a;
  struct rtt *t;
  a = get_mem(sizeof(struct typealias));
  t = get_mem(sizeof(struct rtt));
  a->name = name;
  a->t = l->l;
  a->l = t;
  *a->l = *l;
  register_type(name, "alias_type", a);
  return a;
}
struct rtv *create_alias(struct val *e) {
  struct val *name;
  name = car(e);
  if (name->T != tyIdent) {
    compiler_error(name, "expected alias name");
  }
  lower_create_alias(eval_type(car(cdr(e))), name->V.S);
  return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
}

struct rtt *void_type(struct val *e, void *prop) {
  (void)e, (void)prop;
  return make_rtt(LLVMVoidType(), NULL, NULL, 0);
}
struct rtt *alias_type(struct val *e, void *prop) {
  (void)e;
  return lower_alias_type((struct typealias *)prop);
}
struct rtt *lower_alias_type(struct typealias *a) {
  return make_rtt(a->t, basictypes_alias, a, 0);
}
struct rtt *integer_type(struct val *e, void *prop) {
  (void)e;
  uint64_t a;
  a = (uint64_t)prop;
  return lower_integer_type(a & 0xff, a & 100);
}
struct rtt *lower_integer_type(int size, int is_unsigned) {
  LLVMTypeRef t;
  (void)is_unsigned;
  switch (size) {
  case 1:
    t = LLVMInt1Type();
    break;
  case 8:
    t = LLVMInt8Type();
    break;
  case 16:
    t = LLVMInt16Type();
    break;
  case 32:
    t = LLVMInt32Type();
    break;
  case 64:
    t = LLVMInt64Type();
    break;
  }
  return make_rtt(t, basictypes_integer,
                  ((void *)(uint64_t)(size | (is_unsigned << 8))), 0);
}

struct rtt *lower_pointer_type(struct rtt *t) {
  struct type *p;
  p = get_mem(sizeof(struct type));
  *p = t->t;
  return make_rtt(LLVMPointerType(t->l, 0), basictypes_pointer, p, 0);
}
void expect_same_number(struct rtv *a, struct rtv *b, struct val *errloc) {
  if (a->t.info == basictypes_integer && b->t.info == basictypes_integer) {
    /* TODO: handle signedness correctly */
    if ((a->t.prop.num & 0xff) == (b->t.prop.num & 0xff)) {
      return;
    }
    if ((a->t.prop.num & 0xff) < (b->t.prop.num & 0xff)) {
      *a = *convert_type(a, make_rtt_from_type(LLVMTypeOf(b->v), b->t), 0);
    } else {
      *b = *convert_type(b, make_rtt_from_type(LLVMTypeOf(a->v), a->t), 0);
    }
    return;
  }
  if (a->t.info == basictypes_float && b->t.info == basictypes_float) {
    if (a->t.prop.num == b->t.prop.num) {
      return;
    }
    if (a->t.prop.num < b->t.prop.num) {
      *a = *convert_type(a, make_rtt_from_type(LLVMTypeOf(b->v), b->t), 0);
    } else {
      *b = *convert_type(b, make_rtt_from_type(LLVMTypeOf(a->v), a->t), 0);
    }
    return;
  }
  if (a->t.info == basictypes_float && b->t.info == basictypes_integer) {
    *b = *convert_type(b, unwrap_type(a), 1);
    return;
  }
  if (a->t.info == basictypes_integer && b->t.info == basictypes_float) {
    *a = *convert_type(a, unwrap_type(b), 1);
    return;
  }
  compiler_error(errloc, "expected two numbers");
}

struct rtt *float_type(struct val *e, void *prop) {
  (void)e;
  return lower_float_type((int)(uint64_t)prop);
}
struct rtt *lower_float_type(int size) {
  LLVMTypeRef t;
  switch (size) {
  case 1:
    t = LLVMFloatType();
    break;
  case 2:
    t = LLVMDoubleType();
    break;
  }
  return make_rtt(t, basictypes_float, (void *)(uint64_t)size, 0);
}

struct rtt *pointer_type(struct val *e, void *prop) {
  struct rtt *t;
  (void)prop;
  if (!e) {
    return make_rtt(LLVMPointerType(LLVMInt8Type(), 0), basictypes_pointer,
                    NULL, 0);
  }
  t = eval_type(e);
  return lower_pointer_type(t);
}
struct rtv *ptr_deref(struct val *e) {
  /*
   * TODO: This currently only works for read access.
   * Writing to dereferenced pointers is not yet supported.
   */
  struct val *f;
  struct rtv *v;
  f = car(e);
  if (!is_nil(cdr(e))) {
    compiler_error(e, "more arguments than expected in \"ptr-deref\"");
  }
  v = eval(f);
  if (v->t.info != basictypes_pointer) {
    compiler_error(e, "attempted to dereference a non-pointer");
  }
  if (!v->t.prop.any) {
    compiler_error(e, "attempted to dereference an untyped pointer");
  }
  return make_rtv(LLVMBuildLoad(bldr, v->v, "ptr-deref"),
                  make_rtt_from_type(LLVMGetElementType(LLVMTypeOf(v->v)),
                                     *v->t.prop.type));
}
struct rtv *ptr_to(struct val *e) {
  struct val *f;
  struct funvar *v;
  f = car(e);
  if (!is_nil(cdr(e))) {
    compiler_error(e, "more arguments than expected in \"ptr-to\"");
  }
  v = lookup_in_fun_scope(curfn, f->V.S);
  return copy_rtv(v->v);
}
