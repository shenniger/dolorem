#include "quote.h"

#include <string.h>

#include "basictypes.h"
#include "eval.h"
#include "fun.h"
#include "jit.h"
#include "type.h"

void init_quote() {
  lower_macroproto("quote");
  lower_macroproto("quasiquote");
  lower_macroproto("quasiunquote");
}
void end_quote() {}

static struct rtv *val_rtv(LLVMValueRef a) {
  return make_rtv(a, lower_alias_type(rt_val_type), vfR);
}

static LLVMValueRef fun_cons, fun_nil, fun_int, fun_string, fun_ident;

static LLVMValueRef require_function(const char *name, LLVMTypeRef a) {
  LLVMValueRef fun;
  fun = LLVMGetNamedFunction(mod, name);
  if (!fun) {
    fun = LLVMAddFunction(mod, name, a);
  }
  return fun;
}

static void setup_quote() {
  LLVMTypeRef t[2];
  t[0] = LLVMPointerType(LLVMInt8Type(), 0);
  t[1] = LLVMPointerType(LLVMInt8Type(), 0);
  fun_cons = require_function("cons", LLVMFunctionType(t[1], t, 2, 0));
  fun_nil = require_function("make_nil_val", LLVMFunctionType(t[1], t, 0, 0));
  t[0] = LLVMInt64Type();
  fun_int = require_function("make_int_val", LLVMFunctionType(t[1], t, 1, 0));
  t[0] = LLVMPointerType(LLVMInt8Type(), 0);
  fun_string =
      require_function("make_string_val", LLVMFunctionType(t[1], t, 1, 0));
  fun_ident =
      require_function("make_ident_val", LLVMFunctionType(t[1], t, 1, 0));
}

LLVMValueRef lower_quote(struct val *e, int quasi) {
  LLVMValueRef v;
  switch (e->T) {
  case tyCons: {
    if (is_nil(e)) {
      return LLVMBuildCall(bldr, fun_nil, NULL, 0, "quote_funcall_nil");
    } else {
      LLVMValueRef args[2];
      struct val *f;
      f = car(e);
      if (quasi && f->T == tyIdent && strcmp(f->V.S, "quasiunquote") == 0) {
        struct rtv *res, *n;
        res = prepare_read(eval(car(cdr(e))));
        n = convert_type(res, lower_alias_type(rt_val_type), 0);
        return n->v;
      }
      args[0] = lower_quote(f, quasi);
      args[1] = lower_quote(cdr(e), quasi);
      return LLVMBuildCall(bldr, fun_cons, args, 2, "quote_funcall_cons");
    }
  }
  case tyInt:
    v = LLVMConstInt(LLVMInt64Type(), e->V.I, 0);
    return LLVMBuildCall(bldr, fun_int, &v, 1, "quote_funcall_int");
  case tyIdent:
    v = LLVMBuildGlobalStringPtr(bldr, e->V.S, "quote_ident_literal");
    return LLVMBuildCall(bldr, fun_ident, &v, 1, "quote_funcall_ident");
  case tyString:
    v = LLVMBuildGlobalStringPtr(bldr, e->V.S, "quote_str_literal");
    return LLVMBuildCall(bldr, fun_string, &v, 1, "quote_funcall_str");
  default:
    compiler_error_internal("invalid type in lower_quote: %i", (int)e->T);
  }
}

struct rtv *quote(struct val *e) {
  setup_quote();
  return val_rtv(lower_quote(e, 0));
}
struct rtv *quasiquote(struct val *e) {
  setup_quote();
  return val_rtv(lower_quote(e, 1));
}
struct rtv *quasiunquote(struct val *e) {
  compiler_error(e, "found \"quasiunquote\" outside of \"quasiquote\"");
}
