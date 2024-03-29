#ifndef INCLUDE_H
#define INCLUDE_H

#include "jit.h"
#include "list.h"
#include "type.h"

extern LLVMModuleRef precompiled_module;
extern int enable_precompilation;

void init_include();
void end_include();
struct rtv *include(struct val *e);
struct rtv *lower_include(const char *name);
struct rtv *lower_include_list(const char *filename, struct val *list);

#endif
