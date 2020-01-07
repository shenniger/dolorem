#include "list.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PART_SIZE (1048576)

struct val *car(struct val *e);
struct val *cdr(struct val *e);
struct val *cons(struct val *a, struct val *b);
int is_nil(struct val *e);
struct val *copy_val(struct val a);
const char *expect_ident(struct val *e);
struct val *make_val(unsigned char t);
struct val *make_nil_val();
struct val *make_int_val(long i);
struct val *make_string_val(char *a);
struct val *make_ident_val(char *a);

struct MemAllocator {
  char *At;
  long Left;
  char **Mem;
  long MemSize;
};

static struct MemAllocator memalloc;

void init_alloc() {
  memalloc.Mem = malloc(sizeof(char *));
  assert(!!memalloc.Mem);
  *memalloc.Mem = calloc(PART_SIZE, 1);
  assert(!!*memalloc.Mem);
  memalloc.At = *memalloc.Mem;
  memalloc.Left = PART_SIZE;
  memalloc.MemSize = 1;
}
void *get_mem(long s) {
  if (memalloc.Left < s) {
    char *a;
    a = calloc(PART_SIZE, 1);
    assert(!!a);
    memalloc.At = a;
    memalloc.Left = PART_SIZE;
    memalloc.MemSize++;
    memalloc.Mem = realloc(memalloc.Mem, sizeof(char *) * memalloc.MemSize);
    assert(!!memalloc.Mem);
  }
  memalloc.Left -= s;
  memalloc.At += s;
  return memalloc.At - s;
}
void *more_mem(void *before, long beforesize, long addedsize) {
  if (!before) {
    return get_mem(addedsize);
  }
  assert(memalloc.At == (char *)before + beforesize);
  if (memalloc.Left < addedsize) {
    void *r;
    r = get_mem(beforesize + addedsize);
    memcpy(r, before, beforesize);
    return r;
  }
  memalloc.Left -= addedsize;
  memalloc.At += addedsize;
  return before;
}
void end_alloc() {
  /*
   * The deallocation of memory right before the program execution ends
   * isn't actually necessary and probably should be removed in Release
   * builds, but it is handy for finding actual memory leaks using valgrind.
   */
  long i;
  for (i = 0; i < memalloc.MemSize; ++i) {
    free(memalloc.Mem[i]);
  }
  free(memalloc.Mem);
}

char *print_to_mem(const char *fmt, ...) {
  va_list va, va2;
  char *str;
  int len;
  va_start(va, fmt);
  va_copy(va2, va);
  str = memalloc.At;
  len = vsnprintf(str, memalloc.Left, fmt, va);
  if (len >= memalloc.Left) {
    str = get_mem(len + 1);
    vsprintf(str, fmt, va2);
  } else {
    memalloc.At += len + 1;
    memalloc.Left -= len + 1;
  }
  va_end(va2);
  va_end(va);
  return str;
}
void print_list(struct val *l) {
  struct val list;
  list = *l;
  switch (list.T) {
  case tyCons:
    if (list.V.L) {
      if (list.V.L->car.T == tyCons) {
        fputc('(', stdout);
        print_list(&list.V.L->car);
        fputc(')', stdout);
      } else {
        print_list(&list.V.L->car);
      }
      if (!(list.V.L->cdr.T == tyCons && !list.V.L->cdr.V.L)) {
        fputc(' ', stdout);
        print_list(&list.V.L->cdr);
      }
    }
    break;
  case tyIdent:
    printf("%s", list.V.S);
    break;
  case tyInt:
    printf("%li", list.V.I);
    break;
  case tyFloat:
    printf("%f", list.V.F);
    break;
  case tyString:
    printf("\"%s\"", list.V.S);
    break;
  default:
    compiler_error(&list, "while printing: found invalid list element (T=%i)",
                   list.T);
    break;
  }
}
void print_list_test(struct val list, int depth) {
  int i;
  switch (list.T) {
  case tyCons:
    if (list.V.L) {
      print_list_test(list.V.L->car,
                      list.V.L->car.T == tyCons ? depth + 2 : depth);
      print_list_test(list.V.L->cdr, depth);
    } else {
      fputs("\n", stderr);
    }
    break;
  case tyIdent:
    fprintf(stderr, "%s:", format_source_loc(list));
    for (i = 0; i < depth; ++i) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "\"%s\"\n", list.V.S);
    break;
  case tyInt:
    fprintf(stderr, "%s:", format_source_loc(list));
    for (i = 0; i < depth; ++i) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "%li\n", list.V.I);
    break;
  case tyFloat:
    fprintf(stderr, "%s:", format_source_loc(list));
    for (i = 0; i < depth; ++i) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "%f\n", list.V.F);
    break;
  case tyString:
    fprintf(stderr, "%s:", format_source_loc(list));
    for (i = 0; i < depth; ++i) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "\"%s\" (quoted)\n", list.V.S);
    break;
  default:
    compiler_error(&list, "while printing: found invalid list element (T=%i)",
                   list.T);
    break;
  }
}

_Noreturn void compiler_error(struct val *l, const char *fmt, ...) {
  va_list va;
  fprintf(stderr, "%s: ERR: ", format_source_loc(*l));
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
  exit(1);
}

_Noreturn void compiler_error_internal(const char *fmt, ...) {
  va_list va;
  fputs("<compiler>: ERR: ", stderr);
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
  exit(1);
}

void compiler_hint(struct val *l, const char *fmt, ...) {
  va_list va;
  fprintf(stderr, "%s: hint: ", format_source_loc(*l));
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
}
void compiler_hint_internal(const char *fmt, ...) {
  va_list va;
  fputs("hint: ", stderr);
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
}

char *readFile(const char *name, size_t *len) {
  FILE *in;
  char *s;
  in = fopen(name, "rb");
  if (!in) {
    return NULL;
  }
  fseek(in, 0, SEEK_END);
  *len = ftell(in);
  fseek(in, 0, SEEK_SET);
  s = get_mem(*len + 1);
  if (fread(s, 1, *len, in) != *len) {
    fclose(in);
    return NULL;
  }
  fclose(in);
  s[*len] = 0;
  return s;
}

struct SourceFile {
  const char *Name;
  const char *Content;
  struct SourceFile *Next;
};
static struct SourceFile *firstsource, *lastsource;
static unsigned short atfile;

static struct SourceFile get_source_file(unsigned short idx) {
  struct SourceFile *s;
  unsigned short i;
  if (!idx) {
    struct SourceFile a;
    a.Name = "<gen>";
    a.Content = NULL;
    return a;
  }
  for (i = 1, s = firstsource; s && i != idx; ++i, s = s->Next)
    ;
  return *s;
}

const char *format_source_loc(struct val l) {
  unsigned line;
  const char *lastline;
  const char *stop;
  const char *s;
  struct SourceFile file;
  file = get_source_file(l.FileIdx);
  line = 1;
  lastline = file.Content;
  stop = file.Content + l.CharIdx;
  if (file.Content) {
    for (s = file.Content; *s && s != stop; ++s) {
      if (*s == '\n') {
        ++line;
        lastline = s;
      }
    }
  }
  return print_to_mem("%s:%i:%i", file.Name, line, stop - lastline);
}

static struct val lower_read(char *str, unsigned short fileidx);

struct val *read_string(const char *s, const char *filename) {
  struct SourceFile **n;
  size_t len;
  char *mutcopy;
  len = 0;
  n = lastsource ? &lastsource->Next : &lastsource;
  *n = get_mem(sizeof(struct SourceFile));
  lastsource = *n;
  lastsource->Name = filename;
  lastsource->Content = s;
  len = strlen(s);
  if (!lastsource->Content) {
    struct val li;
    memset(&li, 0, sizeof(struct val));
    compiler_error(&li, "read_string expected non-null string (in \"%s\")",
                   lastsource->Name);
  }
  if (!firstsource) {
    firstsource = lastsource;
  }
  mutcopy = get_mem(len + 1);
  memcpy(mutcopy, lastsource->Content, len);
  return copy_val(lower_read(mutcopy, ++atfile));
}

struct val *read_file(const char *filename) {
  struct SourceFile **n;
  size_t len;
  char *mutcopy;
  len = 0;
  n = lastsource ? &lastsource->Next : &lastsource;
  *n = get_mem(sizeof(struct SourceFile));
  lastsource = *n;
  lastsource->Name = filename;
  lastsource->Content = readFile(filename, &len);
  if (!lastsource->Content) {
    struct val li;
    memset(&li, 0, sizeof(struct val));
    compiler_error(&li, "can not find file: %s", lastsource->Name);
  }
  if (!firstsource) {
    firstsource = lastsource;
  }
  mutcopy = get_mem(len + 1);
  memcpy(mutcopy, lastsource->Content, len);
  return copy_val(lower_read(mutcopy, ++atfile));
}

unsigned int count_len(struct val *l) {
  unsigned int n;
  struct val *li;
  n = 0;
  for (li = l; li->V.L && li->T == tyCons; li = &li->V.L->cdr) {
    ++n;
  }
  return n;
}

static void actual_read(struct val *v, char *s, unsigned short fileidx,
                        char *filebegin) {
  struct val r;
  struct val prototype;
  struct cons *groupb;
  char *lastidentend;
  prototype = *v;
  groupb = NULL;
  lastidentend = NULL;
  r.Reserved = 0;
  r.FileIdx = fileidx;
resume_loop:
  while (*s) {
    switch (*s) {
    case ' ':
    case '\t':
    case '\n':
    case ')':
    case '}':
      ++s;
      break;
    case '(':
    case '{': {
      char *a;
      int parentheses;
      int is_curly;
      is_curly = *s == '{';
      r.T = tyCons;
      r.V.L = NULL;
      r.CharIdx = s - filebegin;
      a = s + 1;
      parentheses = 0;
      for (;;) {
        if (!*s) {
          compiler_error(&r, "missing closing parentheses");
        }
        if (*s == '"') {
          for (; *s != '"'; ++s) {
            if (!*s) {
              compiler_error(&r, "missing closing '\"'");
            }
          }
        } else if (*s == '#') {
          for (; *s != '\n' && *s; ++s)
            ;
        } else if (*s == '(' || *s == '{') {
          ++parentheses;
        } else if (*s == ')' || *s == '}') {
          if (!--parentheses) {
            break;
          }
        }
        ++s;
      }
      *s = 0;
      ++s;
      actual_read(&r, a, fileidx, filebegin);
      if (!is_curly) {
        goto insert_part;
      } else {
        struct cons *progn_cons;
        progn_cons = get_mem(sizeof(struct cons));
        v->V.L = get_mem(sizeof(struct cons));
        v->V.L->car.FileIdx = fileidx;
        v->V.L->car.CharIdx = r.CharIdx;
        v->V.L->car.T = tyCons;
        v->V.L->car.V.L = progn_cons;
        v->V.L->cdr = prototype;
        progn_cons->car.CharIdx = r.CharIdx;
        progn_cons->car.FileIdx = fileidx;
        progn_cons->car.T = tyIdent;
        progn_cons->car.V.S = "scope";
        progn_cons->cdr = r;
        v = &v->V.L->cdr;
        goto resume_loop;
      }
    }
    case '"':
      r.T = tyString;
      r.CharIdx = s - filebegin;
      ++s;
      r.V.S = s;
      for (; *s != '"'; ++s) {
        if (!*s) {
          compiler_error(&r, "missing closing '\"'");
        }
      }
      *s = 0;
      ++s;
      goto insert_part;
    case '#':
      for (; *s != '\n' && *s; ++s)
        ;
      break;
    case ';': {
      struct val copy;
      if (!groupb) {
        r.CharIdx = s - filebegin;
        compiler_error(&r, "unexpected \";\"");
      }
      copy = groupb->car;
      groupb->car.T = tyCons;
      groupb->car.V.L = get_mem(sizeof(struct cons));
      groupb->car.V.L->car = copy;
      groupb->car.V.L->cdr = groupb->cdr;
      groupb->cdr = prototype;
      v = &groupb->cdr;
      groupb = NULL;
      ++s;
    } break;
    default:
      if (*s >= '0' && *s <= '9') {
        int isFloat;
        char *a;
        isFloat = 0;
        for (a = s; *a && *a != ' ' && *a != ')' && *a != '\n' && *a != '\t' &&
                    *a != ';' && *a != '#' && *a != '}';
             ++a) {
          if (*a == '.') {
            isFloat = 1;
          }
        }
        r.T = isFloat ? tyFloat : tyInt;
        r.CharIdx = s - filebegin;
        if (isFloat) {
          r.V.F = strtod(s, NULL);
        } else {
          r.V.I = strtol(s, NULL, 10);
        }
        s = a;
        goto insert_part;
      } else {
        r.T = tyIdent;
        r.CharIdx = s - filebegin;
        r.V.S = s;
        for (; *s && *s != ' ' && *s != '\t' && *s != '\n' && *s != ';' &&
               *s != '#' && *s != '}' && *s != ')';
             ++s) {
          /* for C compatibility */
          if (*s == '-') {
            *s = '_';
          }
        }
        if (lastidentend) {
          *lastidentend = 0;
        }
        lastidentend = s;
        goto insert_part;
      }
    }
  }
  if (lastidentend) {
    *lastidentend = 0;
  }
  return;

insert_part:
  /*
   * This is a kind of tiny micro-function inside of this function.
   * Somebody really needs to build a language where such a kludge is
   * unnecessary for saving stack space…
   *
   * Unfortunately, nobody's doing that.
   */
  v->V.L = get_mem(sizeof(struct cons));
  v->V.L->car = r;
  v->V.L->cdr = prototype;
  if (r.T != tyCons && !groupb) {
    groupb = v->V.L;
  }
  v = &v->V.L->cdr;
  goto resume_loop;
}

static struct val lower_read(char *str, unsigned short fileidx) {
  struct val r;
  r.Reserved = 0;
  r.FileIdx = fileidx;
  r.V.L = NULL;
  r.CharIdx = 0;
  r.T = tyCons;
  actual_read(&r, str, fileidx, str);
  return r;
}
