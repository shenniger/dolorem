#include "structs.h"

#include "eval.h"
#include "fun.h"

#include <alloca.h>
#include <assert.h>
#include <string.h>

struct typeinf *struct_instance;
struct fun *struct_macro;

static const char *print_struct_type(struct type *a) {
  return a->prop.strct->name;
}

void init_structs() {
  struct_instance = register_type_class("struct_instance", print_struct_type);
  lower_macroproto("defstruct");
  lower_macroproto("memb");
  struct_macro = lower_typemacroproto("struct_type");
}
void end_structs() {}

struct rtt *struct_type(struct val *e, void *prop) {
  (void)e;
  return make_rtt(((struct structprop *)prop)->l, struct_instance, prop, 0);
}
struct rtv *defstruct(struct val *e) {
  struct val *memblist, *m;
  struct structprop *p;
  LLVMTypeRef *t;
  long i;
  p = get_mem(sizeof(struct structprop));
  p->name = expect_ident(car(e));
  memblist = car(cdr(e));
  if (memblist->T != tyCons) {
    compiler_error(memblist, "expected list of members");
  }
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(e, "excess elements in \"defstruct\"");
  }
  p->nmemb = count_len(memblist);
  p->memb = get_mem(sizeof(struct structmemb) * p->nmemb);
  for (i = 0, m = memblist; i < p->nmemb; ++i, m = cdr(m)) {
    p->memb[i].type = *eval_type(car(car(m)));
    p->memb[i].name = expect_ident(car(cdr(car(m))));
    if (!is_nil(cdr(cdr(car(m))))) {
      compiler_error(m, "excess elements in struct member");
    }
  }
  /* LLVM */
  t = alloca(sizeof(LLVMTypeRef) * p->nmemb);
  for (i = 0; i < p->nmemb; ++i) {
    t[i] = p->memb[i].type.l;
  }
  p->l = LLVMStructCreateNamed(LLVMGetGlobalContext(), p->name);
  LLVMStructSetBody(p->l, t, p->nmemb, 0);

  register_type(p->name, struct_macro, p);
  return &null_rtv;
}
struct rtv *memb(struct val *e) {
  const char *name;
  struct rtv *o;
  long i;
  /* TODO: structs as r-values!!! */
  o = eval(car(e));
  name = expect_ident(car(cdr(e)));
  assert(o->t.value_flags & vfL);
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(cdr(cdr(e)), "excess arguments in \"memb\"");
  }
  if (o->t.info != struct_instance) {
    compiler_error(car(e), "expected struct, found \"%s\"", print_type(&o->t));
  }
  for (i = 0; i < o->t.prop.strct->nmemb; ++i) {
    if (strcmp(o->t.prop.strct->memb[i].name, name) == 0) {
      return make_rtv(LLVMBuildStructGEP(bldr, o->v, i, "structmemb"),
                      &o->t.prop.strct->memb[i].type, vfL);
    }
  }
  compiler_error(car(e), "struct member \"%s\" not found in struct \"%s\"",
                 name, o->t.prop.strct->name);
}
