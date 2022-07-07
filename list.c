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
int val_is_int(struct val *e);
int val_is_list(struct val *e);
int val_is_ident(struct val *e);
int val_is_float(struct val *e);
int val_is_string(struct val *e);
struct val *copy_val(struct val a);
const char *expect_ident(struct val *e);
struct val *make_val(unsigned char t);
struct val *make_nil_val();
struct val *make_int_val(long i);
struct val *make_string_val(char *a);
struct val *make_ident_val(char *a);
struct val *make_char_val(char i);

static const char *format_source_loc(struct val l);

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
void print_list_to_stdout(struct val *l) { print_list(l, stdout); }
void print_list(struct val *l, FILE *to) {
  struct val list;
  list = *l;
  switch (list.T) {
  case tyCons:
    if (list.V.L) {
      if (list.V.L->car.T == tyCons) {
        fputc('(', to);
        print_list(&list.V.L->car, to);
        fputc(')', to);
      } else {
        print_list(&list.V.L->car, to);
      }
      if (!(list.V.L->cdr.T == tyCons && !list.V.L->cdr.V.L)) {
        fputc(' ', to);
        print_list(&list.V.L->cdr, to);
      }
    }
    break;
  case tyIdent:
    fprintf(to, "%s", list.V.S);
    break;
  case tyInt:
    fprintf(to, "%li", list.V.I);
    break;
  case tyFloat:
    fprintf(to, "%f", list.V.F);
    break;
  case tyString:
    fprintf(to, "\"%s\"", list.V.S);
    break;
  case tyChar:
    fprintf(to, "\'%c\'", (char)list.V.I);
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
  case tyChar:
    fprintf(stderr, "%s:", format_source_loc(list));
    for (i = 0; i < depth; ++i) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "\'%d\' (quoted)\n", (char)list.V.I);
    break;
  default:
    compiler_hint(&list, "while printing: found invalid list element (T=%i)",
                  list.T);
    break;
  }
}

static void print_location_hint(struct val l) {
  if (!l.FileIdx) {
    fprintf(stderr, "near generated code \"");
    print_list(&l, stderr);
    fprintf(stderr, "\"\n");
  } else {
    const char *line;
    char *endline;
    char buf[255]; /* protection to prevent over-long lines
                    * from becoming performance problems */
    long chr;
    get_loc_info(&l, NULL, NULL, &chr, &line);
    strncpy(buf, line, sizeof(buf));
    if ((endline = strchr(buf, '\n'))) {
      *endline = 0;
    }
    fprintf(stderr, "     \x1B[33m%s\x1B[0m\n     ", buf);
    while (chr--) {
      fputc(' ', stderr);
    }
    fprintf(stderr, "\x1B[31m^\x1B[0m\n");
  }
}

_Noreturn void compiler_error(struct val *l, const char *fmt, ...) {
  va_list va;
  fprintf(stderr, "\x1B[36m%s\x1B[0m: \x1B[31mERR:\x1B[0m ",
          format_source_loc(*l));
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
  print_location_hint(*l);
  exit(1);
}

_Noreturn void compiler_error_internal(const char *fmt, ...) {
  va_list va;
  fputs("\x1B[36m<compiler>\x1B[0m: \x1B[31mERR: \x1B[0m", stderr);
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fputs("\n", stderr);
  exit(1);
}

void compiler_hint(struct val *l, const char *fmt, ...) {
  va_list va;
  fprintf(stderr, "\x1B[36m%s\x1B[0m: \x1B[33mhint:\x1B[0m ",
          format_source_loc(*l));
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  if (!l->FileIdx) {
    fprintf(stdout, "near generated code \"");
    print_list(l, stdout);
    fprintf(stdout, "\"\n");
  }
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

static void prepare_string(char *s, struct val *e, char type) {
  char *r, *w;
  for (r = s, w = s; *r;) {
    if (*r == type) {
      for (++r; *r && *r != type; ++r)
        ;
      if (!*r) {
        compiler_error_internal(
            "prepare_string found invalid multi-part string");
      }
      ++r;
      continue;
    }
    if (*r == '\\') {
      switch (*++r) {
      case '\\':
        *w++ = '\\';
        break;
      case '\"':
        *w++ = '\"';
        break;
      case '\'':
        *w++ = '\'';
        break;
      case 'n':
        *w++ = '\n';
        break;
      case 't':
        *w++ = '\t';
        break;
      /* TODO: more control sequences */
      default:
        compiler_error(e, "invalid control sequence \"%c\"", *r);
      }
      ++r;
      continue;
    }
    *w++ = *r++;
  }
  *w = 0;
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
  if (!s) {
    struct SourceFile a;
    a.Name = "<err finding source file name>";
    a.Content = NULL;
    return a;
  }
  return *s;
}

void get_loc_info(struct val *l, const char **name, long *lineno, long *chr,
                  const char **line) {
  const char *lastline;
  const char *stop;
  const char *s;
  long mylineno;
  struct SourceFile file;
  file = get_source_file(l->FileIdx);
  mylineno = 1;
  lastline = file.Content;
  stop = file.Content + l->CharIdx;
  if (file.Content) {
    for (s = file.Content; *s; ++s) {
      if (*s == '\n') {
        ++mylineno;
        lastline = s + 1;
      }
      if (s == stop) {
        break;
      }
    }
  }
  if (chr) {
    *chr = stop - lastline;
  }
  if (name) {
    *name = file.Name;
  }
  if (lineno) {
    *lineno = mylineno;
  }
  if (line) {
    *line = lastline;
  }
}

static const char *format_source_loc(struct val l) {
  const char *name;
  long line, chr;
  get_loc_info(&l, &name, &line, &chr, NULL);
  return print_to_mem("%s:%i:%i", name, line, chr);
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
    compiler_error_internal("read_string expected non-null string (in \"%s\")",
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
    compiler_error_internal("can not find file: %s", lastsource->Name);
  }
  if (!firstsource) {
    firstsource = lastsource;
  }
  mutcopy = get_mem(len + 1);
  memcpy(mutcopy, lastsource->Content, len);
  return copy_val(lower_read(mutcopy, ++atfile));
}

static char *read_string_from_stdin(size_t *len) {
  char *s;
  size_t i, size;
  s = malloc(size = 4092);
  /* TODO: this uses malloc rather than get_mem */
  i = 0;
  while (!feof(stdin)) {
    if (i - 1 >= size) {
      s = realloc(s, size <<= 1);
    }
    i += fread(s + i, 1, size - i - 1, stdin);
  }
  s[i] = 0;
  *len = i;
  return s;
}

struct val *read_stdin() {
  struct SourceFile **n;
  size_t len;
  char *mutcopy;
  len = 0;
  n = lastsource ? &lastsource->Next : &lastsource;
  *n = get_mem(sizeof(struct SourceFile));
  lastsource = *n;
  lastsource->Name = "<stdin>";
  lastsource->Content = read_string_from_stdin(&len);
  if (!lastsource->Content) {
    struct val li;
    memset(&li, 0, sizeof(struct val));
    compiler_error_internal("can not find file: %s", lastsource->Name);
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

static int is_word_boundary(char a) {
  return a == ' ' || a == '\n' || a == '\t' || a == ';' || a == '#' ||
         a == '}' || a == ')';
}

static char *strlastchr_word_boundary(char *a, char x) {
  char *res = NULL;
  while (*a && !is_word_boundary(*a)) {
    if (*a == x) {
      res = a;
    }
    ++a;
  }
  return res;
}

static void transform_identifier(struct val *v) {
  char *colon;
  if ((colon = strlastchr_word_boundary(v->V.S, ':'))) {
    if (colon == v->V.S || colon[1] == '\0' || is_word_boundary(colon[1])) {
      return;
    }
    struct val cl, memb, funcall, cons1, cons2;
    cons1.CharIdx = cons2.CharIdx = cl.CharIdx = memb.CharIdx =
        funcall.CharIdx = v->CharIdx;
    cons1.CharIdx = cons2.CharIdx = cl.FileIdx = memb.FileIdx =
        funcall.FileIdx = v->FileIdx;
    cl.T = funcall.T = memb.T = tyIdent;
    funcall.V.S = "memb";
    cl.V.S = v->V.S;
    memb.V.S = colon + 1;
    *colon = 0;
    v->T = cons1.T = cons2.T = tyCons;
    v->V.L = get_mem(sizeof(struct cons));
    cons1.V.L = get_mem(sizeof(struct cons));
    cons2.V.L = get_mem(sizeof(struct cons));
    cons2.V.L->cdr.T = tyCons;
    cons2.V.L->cdr.V.L = NULL;
    v->V.L->car = funcall;
    if ((*memb.V.S == '-' &&
         !(memb.V.S[1] == ')' || memb.V.S[1] == '}' || memb.V.S[1] == ' ' ||
           memb.V.S[1] == '\n' || memb.V.S[1] == '\t' || memb.V.S[1] == ';' ||
           memb.V.S[1] == '#' || memb.V.S[1] == '\0')) ||
        (*memb.V.S >= '0' && *memb.V.S <= '9')) {
      int isFloat;
      char *a;
      isFloat = 0;
      for (a = memb.V.S; *a && *a != ' ' && *a != ')' && *a != '\n' &&
                         *a != '\t' && *a != ';' && *a != '#' && *a != '}';
           ++a) {
        if (*a == '.') {
          isFloat = 1;
        }
      }
      memb.T = isFloat ? tyFloat : tyInt;
      if (isFloat) {
        memb.V.F = strtod(memb.V.S, NULL);
      } else {
        memb.V.I = strtol(memb.V.S, NULL, 10);
      }
    }
    cons2.V.L->car = memb;
    cons1.V.L->cdr = cons2;
    cons1.V.L->car = cl;
    v->V.L->cdr = cons1;
    transform_identifier(&cons1.V.L->car);
  }
}

static void actual_read(struct val *v, char *s, unsigned short fileidx,
                        char *filebegin) {
  /*
   * Abandon all hope, ye who enter here.
   *
   * No, seriously, please refer to the technical documentation
   * of this code: https://bit.ly/39V5unK
   *
   * And never forget: https://xkcd.com/1421
   */

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
      for (; *s != '"' || s[-1] == '\\'; ++s) {
        if (!*s) {
          compiler_error(&r, "missing closing '\"'");
        }
      }
      ++s;
      while (*s == '\\') {
        for (; *s != '"'; ++s) {
          if (!*s) {
            compiler_error(&r,
                           "expected next part of string literal, found EOF");
          } else if (*s != '\t' && *s != ' ' && *s != '\n' && *s != '\\') {
            compiler_error(&r, "expected next part of string literal or "
                               "whitespace, found something else");
          }
        }
        ++s;
        for (; *s != '"' || s[-1] == '\\'; ++s) {
          if (!*s) {
            compiler_error(&r, "missing closing '\"'");
          }
        }
        ++s;
      }
      s[-1] = 0;
      prepare_string(r.V.S, &r, '"');
      goto insert_part;
    case '\'':
      r.T = tyChar;
      r.CharIdx = s - filebegin;
      ++s;
      r.V.S = s;
      for (; *s != '\''; ++s) {
        if (!*s) {
          compiler_error(&r, "missing closing '\''");
        }
      }
      *s = 0;
      ++s;
      prepare_string(r.V.S, &r, '\'');
      if (r.V.S[1] != 0) {
        compiler_error(&r, "character literal too long");
      }
      r.V.I = *r.V.S;
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
      v->CharIdx = s - filebegin;
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
      if ((*s == '-' &&
           !(s[1] == ')' || s[1] == '}' || s[1] == ' ' || s[1] == '\n' ||
             s[1] == '\t' || s[1] == ';' || s[1] == '#' || s[1] == '\0')) ||
          (*s >= '0' && *s <= '9')) {
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
        transform_identifier(&r);
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
  v->V.L->cdr.CharIdx = r.CharIdx;
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
