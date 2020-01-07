#ifndef QUOTE_H
#define QUOTE_H

#include "list.h"
#include "type.h"

void init_quote();
void end_quote();

struct rtv *quote(struct val *e);
struct rtv *quasiquote(struct val *e);
struct rtv *quasiunquote(struct val *e);

#endif
