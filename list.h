#ifndef LIST_H
#define LIST_H

void init_alloc();
void *get_mem(long s);
void *more_mem(void *before, long beforesize, long addedsize);
char *print_to_mem(const char *fmt, ...);
void end_alloc();

/* reader */
enum { tyCons = 1, tyInt, tyFloat, tyString, tyIdent, tyChar, tyEnd };
union LEVal { /* one word */
  long I;
  double F; /* TODO: use a float on a 32-bit architecture */
  char *S;
  struct cons *L;
};
struct val { /* two words, TODO: only true on 64-bit architectures so far */
  /* value */
  union LEVal V;
  /* type */
  unsigned char T;
  /* mostly for alignment, might be useful in the future though */
  unsigned char Reserved;
  /* for error messages */
  unsigned int CharIdx;
  unsigned short FileIdx;
};
struct cons { /* four words */
  struct val car, cdr;
};

void get_loc_info(struct val *l, const char **name, long *lineno, long *chr,
                  const char **line);
_Noreturn void compiler_error(struct val *l, const char *fmt, ...);
_Noreturn void compiler_error_internal(const char *fmt, ...);
void compiler_hint(struct val *l, const char *fmt, ...);
void compiler_hint_internal(const char *fmt, ...);

struct val *read_string(const char *s, const char *filename);
struct val *read_file(const char *filename);
struct val *read_stdin();

unsigned int count_len(struct val *l);
void print_list(struct val *l);
void print_list_test(struct val l, int depth);

inline struct val *copy_val(struct val a) {
  struct val *b;
  b = (struct val *)get_mem(sizeof(struct val));
  *b = a;
  return b;
}
inline struct val *car(struct val *e) {
  if (e->T != tyCons) {
    compiler_error_internal("can't take \"car\" of non-list");
  }
  if (!e->V.L) {
    return e;
  }
  return copy_val(e->V.L->car);
}
inline struct val *cdr(struct val *e) {
  if (e->T != tyCons) {
    compiler_error_internal("can't take \"cdr\" of non-list");
  }
  if (!e->V.L) {
    return e;
  }
  return copy_val(e->V.L->cdr);
}
inline struct val *make_val(unsigned char t) {
  struct val *l;
  l = (struct val *)get_mem(sizeof(struct val));
  l->T = t;
  return l;
}
inline struct val *make_nil_val() { return make_val(tyCons); }
inline struct val *make_int_val(long i) {
  struct val *r;
  r = make_val(tyInt);
  r->V.I = i;
  return r;
}
inline struct val *make_char_val(char i) {
  struct val *r;
  r = make_val(tyChar);
  r->V.I = i;
  return r;
}
inline struct val *make_string_val(char *a) {
  struct val *r;
  r = make_val(tyString);
  r->V.S = a;
  return r;
}
inline struct val *make_ident_val(char *a) {
  struct val *r;
  r = make_val(tyIdent);
  r->V.S = a;
  return r;
}
inline struct val *cons(struct val *a, struct val *b) {
  struct val e;
  e.T = tyCons;
  e.Reserved = 0;
  e.CharIdx = 0;
  e.FileIdx = 0;
  e.V.L = (struct cons *)get_mem(sizeof(struct cons));
  e.V.L->car = *a;
  e.V.L->cdr = *b;
  return copy_val(e);
}
inline int is_nil(struct val *e) { return e->T == tyCons && !e->V.L; }
inline int val_is_int(struct val *e) { return e->T == tyInt; }
inline int val_is_list(struct val *e) { return e->T == tyCons && e->V.L; }
inline int val_is_ident(struct val *e) { return e->T == tyIdent; }
inline int val_is_float(struct val *e) { return e->T == tyFloat; }
inline int val_is_string(struct val *e) { return e->T == tyString; }
inline const char *expect_ident(struct val *e) {
  if (e->T != tyIdent) {
    compiler_error(e, "expected identifier");
  }
  return e->V.S;
}

#endif
