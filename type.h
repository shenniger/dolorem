#ifndef TYPE_H
#define TYPE_H

#include "hashmap.h"
#include "jit.h"
#include "list.h"
#include <stdint.h>

union typeprop { /* purely for convenience */
  void *any;
  struct funtypeprop *fun;
  struct typealias *alias;
  struct type *type;
  struct structprop *strct;
  struct arrayprop *arr;
  uint64_t num;
};

struct type {
  struct typeinf *info;
  union typeprop prop;
  uint32_t type_flags;
  uint32_t value_flags; /* part of the type since it's in the frontend */
};
enum { vfR = 0, vfL = 1 }; /* L/R value */

enum { tcfExplicit = 1, tcfPrintHints = 2 };

struct rtt { /* run-time type */
  LLVMTypeRef l;
  struct type t;
};

struct rtv { /* run-time value */
  LLVMValueRef v;
  struct type t;
};

typedef const char *(*type_printer_fun)(struct type *t);

struct typeinf {
  const char *name;
  type_printer_fun printer;
};

struct type_name {
  struct fun *macro;
  void *prop;
  const char *name;
};

extern map_t map_type_classes;

struct type_converter {
  struct fun *f;
  struct type_converter *next;
};

extern struct rtv null_rtv;
extern struct type_converter *type_converter_list;
extern struct type_converter *type_converter_last;
struct rtv *register_type_converter(struct val *l);
void lower_register_type_converter(struct fun *f);

struct rtv *convert_equal_types(struct rtv *v, struct rtt *to, int flags);

void init_types();
void end_types();
struct rtt *eval_type(struct val *e);
struct rtv *convert(struct val *e);
struct rtv *convert_type(struct rtv *a, struct rtt *to, int flags);
const char *print_type(struct type *t);
long sizeof_type(struct rtt *a);
void register_type(const char *name, struct fun *macro, void *prop);
struct rtv *prepare_read(struct rtv *a);
struct typeinf *register_type_class(const char *name, type_printer_fun printer);
inline struct rtt *copy_rtt(struct rtt a) {
  struct rtt *b;
  b = (struct rtt *)get_mem(sizeof(struct rtt));
  *b = a;
  return b;
}
inline struct rtv *copy_rtv(struct rtv a) {
  struct rtv *b;
  b = (struct rtv *)get_mem(sizeof(struct rtv));
  *b = a;
  return b;
}
inline struct rtt *make_rtt(LLVMTypeRef r, struct typeinf *info, void *prop,
                            long type_flags) {
  struct rtt t;
  t.l = r;
  t.t.info = info;
  t.t.prop.any = prop;
  t.t.type_flags = type_flags;
  t.t.value_flags = 0;
  return copy_rtt(t);
}
inline struct rtt *make_rtt_from_type(LLVMTypeRef r, struct type t) {
  return make_rtt(r, t.info, t.prop.any, t.type_flags);
}
inline struct rtv *make_rtv(LLVMValueRef v, struct rtt *t,
                            uint32_t value_flags) {
  struct rtv r;
  r.v = v;
  r.t = t->t;
  r.t.value_flags = value_flags;
  return copy_rtv(r);
}
inline struct rtv *make_rtv_from_type(LLVMValueRef v, struct type t,
                                      uint32_t value_flags) {
  struct rtv r;
  r.v = v;
  r.t = t;
  r.t.value_flags = value_flags;
  return copy_rtv(r);
}
inline struct rtv *make_twin_rtv(LLVMValueRef v, struct rtv *old) {
  struct rtv *r;
  r = copy_rtv(*old);
  r->v = v;
  return r;
}
inline LLVMValueRef unwrap_llvm_value(struct rtv *a) { return a->v; }
inline struct rtv *empty_rtv() { return &null_rtv; }
inline struct rtt *unwrap_type(struct rtv *a) {
  if (!a->t.info) {
    return make_rtt_from_type(NULL, a->t);
  }
  if (a->t.value_flags & vfL) {
    return make_rtt_from_type(LLVMGetElementType(LLVMTypeOf(a->v)), a->t);
  }
  return make_rtt_from_type(LLVMTypeOf(a->v), a->t);
}
inline struct type *unwrap_type_t(struct rtv *a) { return &a->t; }
inline LLVMTypeRef unwrap_llvm_type(struct rtt *a) { return a->l; }

#endif
