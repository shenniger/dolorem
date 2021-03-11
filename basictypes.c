#include "basictypes.h"

#include "eval.h"
#include "fun.h"
#include "jit.h"
#include "list.h"
#include "type.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stddef.h>

struct typeinf *basictypes_integer, *basictypes_alias, *basictypes_pointer,
    *basictypes_float, *basictypes_array;

int is_int(struct rtt *a);
int is_float(struct rtt *a);

static const char *print_alias_type(struct type *t) {
  if (t->prop.alias->l) {
    return print_to_mem("%s (aka '%s')", t->prop.alias->name,
                        print_type(&t->prop.alias->l->t));
  }
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
  if (!t->prop.type) {
    return "ptr";
  }
  return print_to_mem("ptr %s", print_type(t->prop.type));
}
static const char *print_array_type(struct type *t) {
  return print_to_mem("array %i %s", t->prop.arr->size, print_type(&t->prop.arr->t));
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
    if (!is_explicit && 0) { /* TODO */
      /*
       * TODO: This rule is too lenient and doesn't handle volatility and
       * constness correctly. Also too strict regarding pointers to aliases
       * correctly.
       */
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
      /*return NULL; */ /* TODO: find a solution for this */
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
      return make_rtv(LLVMBuildSIToFP(bldr, v->v, to->l, "sitofp"), to, vfR);
    } else {
      return make_rtv(LLVMBuildUIToFP(bldr, v->v, to->l, "uitofp"), to, vfR);
    }
  }
  if (v->t.info == basictypes_float && to->t.info == basictypes_integer &&
      is_explicit) {
    if (to->t.prop.num & 0x100) {
      return make_rtv(LLVMBuildFPToSI(bldr, v->v, to->l, "fptosi"), to, vfR);
    } else {
      return make_rtv(LLVMBuildFPToUI(bldr, v->v, to->l, "fptoui"), to, vfR);
    }
  }
  return NULL;
}

static struct fun *alias_macro;

void init_basictypes() {
  struct fun *integer_macro, *float_macro, *pointer_macro, *array_macro;
  basictypes_alias = register_type_class("alias", print_alias_type);
  basictypes_integer = register_type_class("integer", print_integer_type);
  basictypes_pointer = register_type_class("pointer", print_pointer_type);
  basictypes_float = register_type_class("float", print_float_type);
  basictypes_array = register_type_class("array", print_array_type);

  alias_macro = get_mem(sizeof(struct fun));
  alias_macro->name = "alias_type";
  if (hashmap_put(map_funs, "alias_type", alias_macro) != MAP_OK) {
    compiler_error_internal("error inserting function into hashmap: \"%s\"",
                            "alias_type");
  }

  /* for fun.c */
  rt_val_type = lower_create_opaque(LLVMPointerType(LLVMInt8Type(), 0), "val");
  rt_rtt_type = lower_create_opaque(LLVMPointerType(LLVMInt8Type(), 0), "rtt");
  rt_rtv_type = lower_create_opaque(LLVMPointerType(LLVMInt8Type(), 0), "rtv");

  integer_macro = lower_typemacroproto("integer_type");
  pointer_macro = lower_typemacroproto("pointer_type");
  float_macro = lower_typemacroproto("float_type");
  array_macro = lower_typemacroproto("array_type");
  register_type("void", lower_typemacroproto("void_type"), NULL);
  register_type("bool", integer_macro, (void *)(0x100 | 1));
  register_type("char", integer_macro, (void *)(0x000 | 8));
  register_type("i8", integer_macro, (void *)(0x100 | 8));
  register_type("i16", integer_macro, (void *)(0x100 | 16));
  register_type("i32", integer_macro, (void *)(0x100 | 32));
  register_type("i64", integer_macro, (void *)(0x100 | 64));
  register_type("u8", integer_macro, (void *)(0x000 | 8));
  register_type("u16", integer_macro, (void *)(0x000 | 16));
  register_type("u32", integer_macro, (void *)(0x000 | 32));
  register_type("u64", integer_macro, (void *)(0x000 | 64));
  register_type("ptr", pointer_macro, NULL);
  register_type("array", array_macro, NULL);
  register_type("float", float_macro, (void *)1);
  register_type("double", float_macro, (void *)2);

  lower_macroproto("create_alias");
  lower_macroproto("ptr_deref");
  lower_macroproto("ptr_to");

  alias_macro->type.flags = ffTypeMacro;
  alias_macro->type.nparms = 2;
  alias_macro->type.parms = get_mem(sizeof(struct funparm) * 2);
  alias_macro->type.parms[0].name = "args";
  alias_macro->type.parms[0].t = *lower_alias_type(rt_val_type);
  alias_macro->type.parms[1].name = "prop";
  alias_macro->type.parms[1].t = *pointer_type(NULL, NULL);
  alias_macro->type.ret = *lower_alias_type(rt_rtt_type);
  alias_macro->type.funtype = fun_type_to_llvm(alias_macro->type);

  /* for fun.c */
  register_type("funptr", lower_typemacroproto("funptr_type"), NULL);
  lower_macroproto("funproto");
  lower_macroproto("defun");
  lower_macroproto("macroproto");
  lower_macroproto("defmacro");
  lower_macroproto("typemacroproto");
  lower_macroproto("deftypemacro");
  lower_macroproto("compiledfunction");
  lower_macroproto("convert"); /* from type.c */

  lower_register_type_converter(
      lower_typeconverterproto("convert_equal_types"));
  lower_register_type_converter(
      lower_typeconverterproto("convert_pointer_types"));
  lower_register_type_converter(
      lower_typeconverterproto("convert_alias_unwrap"));
  lower_register_type_converter(lower_typeconverterproto("convert_numbers"));
}

void end_basictypes() {}
struct typealias *lower_create_opaque(LLVMTypeRef t, const char *name) {
  struct typealias *a;
  a = get_mem(sizeof(struct typealias));
  a->name = name;
  a->t = t;
  a->l = NULL;
  register_type(name, alias_macro, a);
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
  register_type(name, alias_macro, a);
  return a;
}
struct rtv *create_alias(struct val *e) {
  struct val *name;
  name = car(e);
  if (name->T != tyIdent) {
    compiler_error(name, "expected alias name");
  }
  lower_create_alias(eval_type(car(cdr(e))), name->V.S);
  return &null_rtv;
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
  return lower_integer_type(a & 0xff, a & 0x100);
}
struct rtt *lower_integer_type(int size, int is_unsigned) {
  LLVMTypeRef t;
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
  default:
    assert(!"invalid integer type request");
  }
  return make_rtt(t, basictypes_integer,
                  ((void *)(uint64_t)(size | ((!!is_unsigned) << 8))), 0);
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
  struct val *f;
  struct rtv *v;
  f = car(e);
  if (!is_nil(cdr(e))) {
    compiler_error(e, "more arguments than expected in \"ptr-deref\"");
  }
  v = prepare_read(eval(f));
  if (v->t.info != basictypes_pointer) {
    compiler_error(e, "attempted to dereference a non-pointer (of type \"%s\")",
                   print_type(&v->t));
  }
  if (!v->t.prop.any) {
    compiler_error(e, "attempted to dereference an untyped pointer");
  }
  return make_rtv(v->v, make_rtt_from_type(NULL, *v->t.prop.type), vfL);
}
struct rtv *ptr_to(struct val *e) {
  struct rtv *f;
  f = eval(car(e));
  if (!is_nil(cdr(e))) {
    compiler_error(e, "more arguments than expected in \"ptr-to\"");
  }
  if (!(f->t.value_flags & vfL)) {
    compiler_error(e, "expected l-value");
  }
  return lower_ptr_to(f);
}
struct rtv *lower_ptr_to(struct rtv *f) {
  struct rtv *r;
  r = copy_rtv(*f);
  r->t.value_flags = vfR;
  if (f->t.info == basictypes_array) {
    /* TODO: make a proper way to access arrays */
    LLVMValueRef indices[2];
    /* TODO: Int64: add compatibility for 32-bit arch */
    indices[0] = LLVMConstInt(LLVMInt64Type(), 0, 0);
    indices[1] = LLVMConstInt(LLVMInt64Type(), 0, 0);
    return make_rtv(
        LLVMBuildGEP2(bldr, LLVMGetElementType(LLVMTypeOf(f->v)), f->v, indices,
                      sizeof(indices) / sizeof(LLVMValueRef), "arrayusage"),
        lower_pointer_type(make_rtt_from_type(
            LLVMGetElementType(LLVMTypeOf(f->v)), f->t.prop.arr->t)),
        vfR);
  }
  return make_rtv(
      f->v, lower_pointer_type(make_rtt_from_type(LLVMTypeOf(f->v), f->t)),
      vfR);
}

/* arrays */
struct rtt *lower_array_type(struct rtt *t, long size) {
  struct arrayprop *p;
  p = get_mem(sizeof(struct arrayprop));
  p->t = t->t;
  p->size = size;
  return make_rtt(LLVMArrayType(t->l, size), basictypes_array, p, 0);
}
struct rtt *array_type(struct val *e, void *prop) {
  struct rtt *t;
  (void)prop;
  if (!e) {
    return make_rtt(LLVMPointerType(LLVMInt8Type(), 0), basictypes_pointer,
                    NULL, 0);
  }
  if (!val_is_int(car(e))) {
    compiler_error(e, "expected number constant, found something else");
    /* allow for constants */
  }
  t = eval_type(cdr(e));
  return lower_array_type(t, car(e)->V.I);
}
struct rtv *memb_array(struct rtv *array, struct val *index) {
  struct rtv *ptr, *idx;
  ptr = lower_ptr_to(array);
  idx = convert_type(eval(index), lower_integer_type(64, 0),
                     0); /* TODO: 32-bit */
  return make_rtv(LLVMBuildGEP2(bldr, LLVMGetElementType(LLVMTypeOf(ptr->v)),
                                ptr->v, &idx->v, 1, "indexadd"),
                  make_rtt_from_type(LLVMTypeOf(ptr->v), *ptr->t.prop.type),
                  vfL);
}
