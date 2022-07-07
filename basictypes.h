#ifndef BASICTYPES_H
#define BASICTYPES_H

#include "jit.h"
#include "list.h"
#include "type.h"

struct typealias {
  struct rtt *l; /* may be NULL */
  LLVMTypeRef t;
  const char *name;
};

struct arrayprop {
  long size;
  struct type t;
};

extern struct typeinf *basictypes_integer, *basictypes_alias,
    *basictypes_pointer, *basictypes_float, *basictypes_array;

void init_basictypes();
void end_basictypes();

struct rtt *void_type(struct val *e, void *prop);
inline int is_void(struct rtt *a) { return a->t.info == NULL; }

struct typealias *lower_create_opaque(LLVMTypeRef t, const char *name);
struct typealias *lower_create_alias(struct rtt *l, const char *name);
struct rtv *create_alias(struct val *e);
struct rtt *alias_type(struct val *e, void *prop);
struct rtt *unwrap_alias(struct rtt *v);
inline int is_alias(struct rtt *a) { return a->t.info == basictypes_alias; }
inline int is_opaque_alias(struct rtt *a) {
  return a->t.info == basictypes_alias && !a->t.prop.alias->l;
}

void expect_same_number(struct rtv *a, struct rtv *b, struct val *errloc);

struct rtt *integer_type(struct val *e, void *prop);
struct rtt *lower_integer_type(int size, int is_unsigned);
inline int is_int(struct rtt *a) { return a->t.info == basictypes_integer; }

struct rtt *float_type(struct val *e, void *prop);
struct rtt *lower_float_type(int size);
inline int is_float(struct rtt *a) { return a->t.info == basictypes_float; }

/* pointers */
struct rtt *lower_pointer_type(struct rtt *t);
struct rtt *pointer_type(struct val *e, void *prop);
struct rtv *ptr_deref(struct val *e);
struct rtv *ptr_to(struct val *e);
struct rtv *lower_ptr_to(struct rtv *e);
inline int is_pointer(struct rtt *a) { return a->t.info == basictypes_pointer; }
inline int is_str(struct rtt *a) {
  return a->t.info == basictypes_pointer &&
         a->t.prop.type->info == basictypes_integer &&
         a->t.prop.type->type_flags == 8;
}
struct rtt *unwrap_pointer(struct rtt *a);

struct rtt *lower_alias_type(struct typealias *a);

/* arrays */
struct rtt *lower_array_type(struct rtt *t, long size);
struct rtt *array_type(struct val *e, void *prop);
struct rtv *memb_array(struct rtv *array, struct val *idx);
inline int is_array(struct rtt *a) { return a->t.info == basictypes_array; }

#endif
