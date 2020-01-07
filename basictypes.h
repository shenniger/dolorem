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

extern struct typeinf *basictypes_integer, *basictypes_alias,
    *basictypes_pointer;

void init_basictypes();
void end_basictypes();

struct rtt *void_type(struct val *e, void *prop);

struct typealias *lower_create_opaque(LLVMTypeRef t, const char *name);
struct typealias *lower_create_alias(struct rtt *l, const char *name);
struct rtv *create_alias(struct val *e);
struct rtt *alias_type(struct val *e, void *prop);

struct rtt *integer_type(struct val *e, void *prop);
struct rtt *lower_integer_type(int size, int is_unsigned);
void expect_same_integer(struct rtv *a, struct rtv *b, struct val *errloc);

/* pointers */
struct rtt *lower_pointer_type(struct rtt *t);
struct rtt *pointer_type(struct val *e, void *prop);
struct rtv *ptr_deref(struct val *e);
struct rtv *ptr_to(struct val *e);

struct rtt *lower_alias_type(struct typealias *a);

#endif
