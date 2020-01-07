#ifndef GLOBAL_H
#define GLOBAL_H

#include "hashmap.h"
#include "list.h"
#include "type.h"

struct global {
  const char *name;
  struct rtt *type;
};

extern map_t map_globals;

void init_global();
void end_global();

void lower_defglobal(const char *name, struct rtt *type, int isextern);
struct rtv *defglobal(struct val *l);
struct rtv *external_global(struct val *l);

#endif
