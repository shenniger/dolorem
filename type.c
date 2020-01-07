#include "type.h"

#include "eval.h"
#include <assert.h>
#include <string.h>

map_t map_types;
map_t map_type_classes;

struct type_converter *type_converter_list;
struct type_converter *type_converter_last;

struct rtt *make_rtt(LLVMTypeRef r, struct typeinf *info, void *prop,
                     long type_flags);
struct rtv *make_rtv(LLVMValueRef v, struct rtt *t);
struct rtt *copy_rtt(struct rtt a);
struct rtv *copy_rtv(struct rtv a);
LLVMValueRef unwrap_llvm_value(struct rtv *a);
struct rtt *make_rtt_from_type(LLVMTypeRef r, struct type t);
struct rtv *empty_rtv();
struct rtv *make_twin_rtv(LLVMValueRef v, struct rtv *old);
struct rtt *unwrap_type(struct rtv *a);
LLVMTypeRef unwrap_llvm_type(struct rtt *a);

struct rtv *convert_equal_types(struct rtv *v, struct rtt *to,
                                int is_explicit) {
  (void)is_explicit;
  /* TODO: This rule is too lenient and doesn't handle volatility and constness
   * correctly. */
  if (v->t.info == to->t.info &&
      memcmp(&v->t.prop, &to->t.prop, sizeof(union typeprop)) == 0) {
    return v;
  }
  return NULL;
}

void init_types() {
  map_types = hashmap_new();
  map_type_classes = hashmap_new();
  assert(map_type_classes);
  lower_register_type_converter("convert_equal_types");
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
void register_type(const char *name, const char *macroname, void *prop) {
  struct type_name *n;
  void *dummy;
  n = get_mem(sizeof(struct type_name));
  n->name = name;
  n->macroname = macroname;
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
  return call_type_macro(n->macroname, l, n->prop);
}

struct rtv *register_type_converter(struct val *l) {
  struct val *name;
  name = car(l);
  if (!is_nil(cdr(l))) {
    compiler_error(cdr(l), "excess elements in \"register-type-converter\"");
  }
  if (name->T != tyIdent) {
    compiler_error(name, "expected identifier");
  }
  lower_register_type_converter(name->V.S);
  return make_rtv(NULL, make_rtt(NULL, NULL, NULL, 0));
}
void lower_register_type_converter(const char *name) {
  if (!type_converter_last) {
    type_converter_list = get_mem(sizeof(struct type_converter));
    type_converter_last = type_converter_list;
  } else {
    type_converter_last->next = get_mem(sizeof(struct type_converter));
    type_converter_last = type_converter_last->next;
  }
  type_converter_last->name = name;
}

struct rtv *convert_type(struct rtv *a, struct rtt *to, int is_explicit_cast) {
  struct type_converter *t;
  struct rtv *r;
  for (t = type_converter_list; t; t = t->next) {
    if ((r = call_type_converter(t->name, a, to, is_explicit_cast))) {
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
