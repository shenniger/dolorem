#ifndef STRUCTS_H
#define STRUCTS_H

#include "jit.h"
#include "list.h"
#include "type.h"

struct typeinf *struct_instance;

struct structmemb {
  const char *name;
  struct rtt type;
};

struct structprop {
  const char *name;
  LLVMTypeRef l;
  struct structmemb *memb;
  long nmemb;
};

void init_structs();
void end_structs();

struct rtt *struct_type(struct val *e, void *prop);
struct rtv *defstruct(struct val *e);
struct rtv *memb(struct val *e);

#endif
