#ifndef VAR_H
#define VAR_H

#include "list.h"
#include "type.h"

void init_var();
void end_var();

/* adds a variable to the current scope */
struct rtv *add_variable(const char *name, struct rtt *type);

void lower_assign(const char *name, struct rtv *value);
struct rtv *assign(struct val *e);

#endif
