#include "type.h"

#include "eval.h"
#include "llvmext.h"
#include <assert.h>
#include <llvm-c/Core.h>
#include <string.h>

map_t map_types;
map_t map_type_classes;

struct rtv null_rtv;

struct type_converter *type_converter_list;
struct type_converter *type_converter_last;

struct rtv *make_rtv(LLVMValueRef v, struct rtt *t, uint32_t value_flags);
struct rtt *copy_rtt(struct rtt a);
struct rtv *copy_rtv(struct rtv a);
LLVMValueRef unwrap_llvm_value(struct rtv *a);
struct rtt *make_rtt_from_type(LLVMTypeRef r, struct type t);
struct rtv *empty_rtv();
struct rtv *make_twin_rtv(LLVMValueRef v, struct rtv *old);
struct rtt *unwrap_type(struct rtv *a);
LLVMTypeRef unwrap_llvm_type(struct rtt *a);
struct type *unwrap_type_t(struct rtv *a);
struct rtv *make_rtv_from_type(LLVMValueRef v, struct type t,
                               uint32_t value_flags);

struct rtv *convert_equal_types(struct rtv *v, struct rtt *to,
                                int is_explicit) {
  (void)is_explicit;
  /* TODO: This rule is too lenient and doesn't handle volatility and constness
   * correctly. */
  if (v->t.info == to->t.info) {
    if (v->t.info == funptr) {
      return v; /* TODO!!! */
    }
    if (memcmp(&v->t.prop, &to->t.prop, sizeof(union typeprop)) == 0) {
      return v;
    }
  }
  return NULL;
}

void init_types() {
  map_types = hashmap_new();
  map_type_classes = hashmap_new();
  assert(map_type_classes);
}
void end_types() {
  hashmap_free(map_type_classes);
  hashmap_free(map_types);
}
struct typeinf *register_type_class(const char *name,
                                    type_printer_fun printer) {
  struct typeinf *n;
  void *dummy;
  n = get_mem(sizeof(struct typeinf));
  n->name = name;
  n->printer = printer;
  if (hashmap_get(map_type_classes, name, &dummy) == MAP_OK) {
    compiler_error_internal(
        "cannot \"register-type-class\" \"%s\" because it already exists",
        name);
  }
  if (hashmap_put(map_type_classes, name, n) != MAP_OK) {
    compiler_error_internal("cannot \"register-type-class\" \"%s\"", name);
  }
  return n;
}
void register_type(const char *name, struct fun *macro, void *prop) {
  struct type_name *n;
  void *dummy;
  n = get_mem(sizeof(struct type_name));
  n->name = name;
  n->macro = macro;
  n->prop = prop;
  if (hashmap_get(map_types, name, &dummy) == MAP_OK) {
    compiler_error_internal(
        "cannot \"register-type\" \"%s\" because it already exists", name);
  }
  if (hashmap_put(map_types, name, n) != MAP_OK) {
    compiler_error_internal("cannot \"register-type\" \"%s\"", name);
  }
}

struct rtv *convert(struct val *l) {
  struct val *type, *expr;
  type = car(l);
  expr = car(cdr(l));
  if (!is_nil(cdr(cdr(l)))) {
    compiler_error(cdr(l), "excess elements in \"convert\"");
  }
  return convert_type(eval(expr), eval_type(type), 1);
}

struct rtt *eval_type(struct val *e) {
  struct type_name *n;
  struct val *l;
  l = NULL;
  if (e->T == tyCons) {
    l = cdr(e);
    e = car(e);
  }
  if (e->T != tyIdent) {
    compiler_error(e, "expected type");
  }
  if (hashmap_get(map_types, e->V.S, (void **)&n) != MAP_OK) {
    compiler_error(e, "unknown type: \"%s\"", e->V.S);
  }
  return call_type_macro(n->macro, l, n->prop);
}

struct rtv *prepare_read(struct rtv *v) {
  if (v->t.value_flags & vfL) {
    return make_rtv_from_type(LLVMBuildLoad(bldr, v->v, "lval_load"), v->t,
                              vfR);
  }
  return v;
}

struct rtv *register_type_converter(struct val *l) {
  struct val *name;
  struct fun *f;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"register-type-converter\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  if (!((f = lookup_fun(name->V.S)))) {
    compiler_error(name, "unknown function: \"%s\"", name->V.S);
  }
  lower_register_type_converter(f);
  return &null_rtv;
}
void lower_register_type_converter(struct fun *f) {
  if (!type_converter_last) {
    type_converter_list = get_mem(sizeof(struct type_converter));
    type_converter_last = type_converter_list;
  } else {
    type_converter_last->next = get_mem(sizeof(struct type_converter));
    type_converter_last = type_converter_last->next;
  }
  type_converter_last->f = f;
}

struct rtt *make_rtt(LLVMTypeRef r, struct typeinf *info, void *prop,
                     long type_flags);
struct rtv *convert_type(struct rtv *a, struct rtt *to, int is_explicit_cast) {
  struct type_converter *t;
  struct rtv *r;
  struct rtv *from;
  from = prepare_read(a);
  for (t = type_converter_list; t; t = t->next) {
    if ((r = call_type_converter(t->f, from, to, is_explicit_cast))) {
      return r;
    }
  }
  return NULL;
}
const char *print_type(struct type *t) {
  if (!t->info) {
    return "void";
  }
  return t->info->printer(t);
}
long sizeof_type(struct rtt *t) { return GetTypeSize(typemod, t->l); }
