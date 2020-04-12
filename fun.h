#ifndef FUN_H
#define FUN_H

#include <stddef.h>

#include "hashmap.h"
#include "list.h"
#include "type.h"

struct funparm {
  struct rtt t;
  const char *name;
};

struct funtypeprop {
  struct rtt ret;
  struct funparm *parms;
  long nparms;
  long flags;
  LLVMTypeRef funtype;
};

enum { ffImplemented = 1, ffMacro = 2, ffTypeMacro = 4, ffTypeConverter = 8 };

struct funvar {
  unsigned int hashed_name;
  const char *name;
  struct rtv v;
  struct rtt t;
  struct funvar *last;
};

struct funscope {
  struct funvar *vars;
  struct funscope *parent;
};

struct fun {
  struct funtypeprop type;
  const char *name;
  struct funscope *scope;
  uint64_t ptr;
};

struct aliasres {
  struct fun *f;
  struct aliasres *next;
};

struct alias {
  struct aliasres *r;
  const char *name;
};

extern map_t map_funs;
extern struct fun *curfn;
extern LLVMValueRef curllvmfn;

void init_fun();
void end_fun();

extern struct typealias *rt_val_type, *rt_rtt_type, *rt_rtv_type;
extern struct typeinf *funptr;

LLVMTypeRef fun_type_to_llvm(struct funtypeprop a);
void fun_set_proper_parm_names(struct fun *f, LLVMValueRef fun);

struct fun *lookup_fun(const char *name);
struct funvar *lookup_in_fun_scope(struct fun *f, const char *name);

struct rtv *funproto(struct val *l);
struct rtv *defun(struct val *l);
struct fun *lower_macroproto(const char *name);
struct rtv *macroproto(struct val *l);
struct rtv *defmacro(struct val *l);
struct fun *lower_typemacroproto(const char *name);
struct rtv *typemacroproto(struct val *l);
struct rtv *deftypemacro(struct val *l);
struct fun *lower_typeconverterproto(const char *name);
struct rtv *typeconverterproto(struct val *l);
struct rtv *deftypeconverter(struct val *l);
struct rtv *compiledfunction(struct val *l);

void funbody(struct fun *f, struct val *body);

#endif
