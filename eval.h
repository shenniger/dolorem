#ifndef EVAL_H
#define EVAL_H

#include "fun.h"
#include "list.h"
#include "type.h"

void init_eval();
void end_eval();
struct rtv *eval(struct val *e);
struct rtv *scope(struct val *e);
struct rtv *progn(struct val *e);
struct rtv *call_funptr(struct val *e);
struct rtv *funptr_to(struct val *e);
struct rtv *make_int_const(long i);
struct rtv *funcall(struct fun *a, struct val *args);
struct rtv *call_fun_macro(const char *name, struct val *e);
struct rtt *call_type_macro(const char *name, struct val *e, void *prop);
struct rtv *call_type_converter(const char *name, struct rtv *value,
                                struct rtt *to, int is_explicit_cast);

#endif
