#include "structs.h"

#include "basictypes.h"
#include "eval.h"
#include "fun.h"
#include "type.h"

#include <alloca.h>
#include <assert.h>
#include <llvm-c/Core.h>
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

static long count_struct_len(struct val *l, long *memb_len) {
  long n;
  struct val *li;
  n = 0;
  *memb_len = 0;
  for (li = l; li->V.L && li->T == tyCons; li = &li->V.L->cdr) {
    ++n;
    if (val_is_ident(car(car(li))) && strcmp(car(car(li))->V.S, "union") == 0) {
      *memb_len += count_len(car(cdr(car(li))));
    } else {
      ++*memb_len;
    }
  }
  return n;
}
struct rtt *struct_type(struct val *e, void *prop) {
  (void)e;
  return make_rtt(((struct structprop *)prop)->l, struct_instance, prop, 0);
}
struct rtv *defstruct(struct val *e) {
  struct val *memblist, *m;
  struct structprop *p;
  LLVMTypeRef *t;
  long i, j;
  long llvmlen;
  p = get_mem(sizeof(struct structprop));
  p->name = expect_ident(car(e));
  memblist = car(cdr(e));
  if (memblist->T != tyCons) {
    compiler_error(memblist, "expected list of members");
  }
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(e, "excess elements in \"defstruct\"");
  }

  llvmlen = count_struct_len(memblist, &p->nmemb);
  p->memb = get_mem(sizeof(struct structmemb) * p->nmemb);
  t = alloca(sizeof(LLVMTypeRef) * llvmlen);

  for (i = 0, j = 0, m = memblist; i < p->nmemb; ++i, ++j, m = cdr(m)) {
    if (val_is_ident(car(car(m))) && strcmp(car(car(m))->V.S, "union") == 0) {
      /* call the FCA -- they're trying to form a ..., a ...., a UNIOOOOON! */
      struct val *n;
      long first, last;
      long longest, longest_size;
      first = i;
      for (n = car(cdr(car(m))); !is_nil(n); n = &n->V.L->cdr, ++i) {
        p->memb[i].type = *eval_type(car(car(n)));
        p->memb[i].name = expect_ident(car(cdr(car(n))));
        if (!is_nil(cdr(cdr(car(n))))) {
          compiler_error(n, "excess elements in union member");
        }
      }
      last = i;
      /* find longest member */
      longest_size = 0;
      longest = -1;
      for (i = first; i < last; ++i) {
        if (sizeof_type(&p->memb[i].type) > longest_size) {
          longest_size = sizeof_type(&p->memb[i].type);
          longest = i;
        }
      }
      if (longest == -1) {
        compiler_error(m, "empty union forbidden");
      }
      for (i = first; i < last; ++i) {
        p->memb[i].unionmain = longest;
        p->memb[longest].llvmnum = j;
      }
      t[j] = p->memb[longest].type.l;
      p->memb[longest].llvmnum = j;
      i = last - 1;
    } else {
      p->memb[i].type = *eval_type(car(car(m)));
      p->memb[i].name = expect_ident(car(cdr(car(m))));
      p->memb[i].unionmain = -1;
      if (!is_nil(cdr(cdr(car(m))))) {
        compiler_error(m, "excess elements in struct member");
      }
      t[j] = p->memb[i].type.l;
      p->memb[i].llvmnum = j;
    }
  }
  p->l = LLVMStructCreateNamed(LLVMGetGlobalContext(), p->name);
  LLVMStructSetBody(p->l, t, llvmlen, 0);

  register_type(p->name, struct_macro, p);
  return &null_rtv;
}
struct rtv *memb(struct val *e) {
  const char *name;
  struct rtv *o;
  long i;
  /* TODO: structs as r-values!!! */
  o = eval(car(e));
  if (o->t.info == basictypes_array) {
    return memb_array(o, car(cdr(e)));
  }
  if (o->t.info != struct_instance) {
    compiler_error(car(e), "expected struct, found \"%s\"", print_type(&o->t));
  }
  name = expect_ident(car(cdr(e)));
  assert(o->t.value_flags & vfL);
  if (!is_nil(cdr(cdr(e)))) {
    compiler_error(cdr(cdr(e)), "excess arguments in \"memb\"");
  }
  for (i = 0; i < o->t.prop.strct->nmemb; ++i) {
    if (strcmp(o->t.prop.strct->memb[i].name, name) == 0) {
      struct structmemb *memb = &o->t.prop.strct->memb[i];
      if (memb->unionmain == -1) {
        return make_rtv(
            LLVMBuildStructGEP(bldr, o->v, memb->llvmnum, "structmemb"),
            &o->t.prop.strct->memb[i].type, vfL);
      } else {
        return make_rtv(
            LLVMBuildBitCast(
                bldr,
                LLVMBuildStructGEP(bldr, o->v, memb->llvmnum, "structmemb"),
                LLVMPointerType(o->t.prop.strct->memb[i].type.l, 0),
                "unionmemb"),
            &o->t.prop.strct->memb[memb->unionmain].type, vfL);
      }
    }
  }
  compiler_error(car(e), "struct member \"%s\" not found in struct \"%s\"",
                 name, o->t.prop.strct->name);
}
