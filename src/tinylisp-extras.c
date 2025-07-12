/* tinylisp-extras.c with the article's extras by Robert A. van Engelen 2025 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* we only need two types to implement a Lisp interpreter:
        I    unsigned integer (either 16 bit, 32 bit or 64 bit unsigned)
        L    Lisp expression (double with NaN boxing)
   I variables and function parameters are named as follows:
        i    any unsigned integer, e.g. a NaN-boxed ordinal value
        t    a NaN-boxed tag
   L variables and function parameters are named as follows:
        x,y  any Lisp expression
        n    number
        t    list
        f,   function, a lambda closure or Lisp primitive
        p    pair, a cons of two Lisp expressions
        e,d  environment, a list of pairs, e.g. created with (define v x)
        v    the name of a variable (an atom) or a list of variables */
#define I unsigned
#define L double

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) *(unsigned long long*)&x>>48

/* address of the atom heap is at the bottom of the cell stack */
#define A (char*)cell

/* number of cells for the shared stack and atom heap, increase N as desired */
#define N 8192

/* section 12: adding readline with history */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in = NULL;
char buf[40],see = ' ',*ptr = "",*line = NULL,ps[20];

/* section 14: error handling and exceptions */
#include <setjmp.h>
#include <signal.h>
jmp_buf jb;
L err(I i) { longjmp(jb,i); }

/* section 4: constructing Lisp expressions */
I hp = 0,sp = N;
I ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd;
L cell[N],nil,tru,env;
L box(I t,I i) { L x; *(unsigned long long*)&x = (unsigned long long)t<<48|i; return x; }
I ord(L x) { return *(unsigned long long*)&x; }
L num(L n) { return n; }
I equ(L x,L y) { return *(unsigned long long*)&x == *(unsigned long long*)&y; }
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp && (hp += strlen(strcpy(A+i,s))+1) > sp<<3) err(4);
 return box(ATOM,i);
}
L cons(L x,L y) { cell[--sp] = x; cell[--sp] = y; if (hp > sp<<3) err(4); return box(CONS,sp); }
L car(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)+1] : err(1); }
L cdr(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)] : err(1); }
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,equ(e,env) ? nil : e))); }
L macro(L v,L x) { return box(MACR,ord(cons(v,x))); }
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e); return T(e) == CONS ? cdr(car(e)) : err(2); }
I not(L x) { return T(x) == NIL; }
I let(L x) { return !not(x) && !not(cdr(x)); }
L eval(L,L),Read(),parse(); void print(L); /* forward proto declarations */

/* section 16.1: replacing recursion with loops */
L evlis(L t,L e) {
 L s,*p;
 for (s = nil,p = &s; T(t) == CONS; p = cell+sp,t = cdr(t)) *p = cons(eval(car(t),e),nil);
 if (T(t) == ATOM) *p = assoc(t,e);
 return s;
}

/* section 16.4: optimizing the Lisp primitives */
L evarg(L *t,L *e,I *a) {
 L x;
 if (T(*t) == ATOM) *t = assoc(*t,*e),*a = 1;
 x = car(*t); *t = cdr(*t);
 return *a ? x : eval(x,*e);
}

/* section 6 Lisp primitives (optimized with evarg per section 16.4) */
L f_eval(L t,L *e) { I a = 0; return evarg(&t,e,&a); }
L f_quote(L t,L *_) { return car(t); }
L f_cons(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return cons(x,evarg(&t,e,&a)); }
L f_car(L t,L *e) { I a = 0; return car(evarg(&t,e,&a)); }
L f_cdr(L t,L *e) { I a = 0; return cdr(evarg(&t,e,&a)); }
L f_add(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n += evarg(&t,e,&a); return num(n); }
L f_sub(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n -= evarg(&t,e,&a); return num(n); }
L f_mul(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n *= evarg(&t,e,&a); return num(n); }
L f_div(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n /= evarg(&t,e,&a); return num(n); }
L f_int(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n<1e16 && n>-1e16 ? (long long)n : n; }
L f_lt(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n - evarg(&t,e,&a) < 0 ? tru : nil; }
L f_eq(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return equ(x,evarg(&t,e,&a)) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return T(x) == CONS ? tru : nil; }
L f_not(L t,L *e) { I a = 0; return not(evarg(&t,e,&a)) ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; while (!not(t) && not(x)) x = evarg(&t,e,&a); return x; }
L f_and(L t,L *e) { I a = 0; L x = tru; while (!not(t) && !not(x)) x = evarg(&t,e,&a); return x; }
L f_cond(L t,L *e) { while (!not(t) && not(eval(car(car(t)),*e))) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { return car(cdr(not(eval(car(t),*e)) ? cdr(t) : t)); }
L f_leta(L t,L *e) { for (;let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),*e),*e); return car(t); }
L f_lambda(L t,L *e) { return closure(car(t),car(cdr(t)),*e); }
L f_define(L t,L *e) { env = pair(car(t),eval(car(cdr(t)),*e),env); return car(t); }

/* section 11: additional Lisp primitives (optimized with evarg per section 16.4) */
L f_assoc(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return assoc(x,evarg(&t,e,&a)); }
L f_env(L _,L *e) { return *e; }
L f_let(L t,L *e) {
 L d = *e;
 for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),d),*e);
 return car(t);
}
L f_letreca(L t,L *e) {
 L *x;
 for (; let(t); t = cdr(t)) *e = pair(car(car(t)),nil,*e),x = &cell[sp+2],*x = eval(car(cdr(car(t))),*e);
 return car(t);
}
L f_setq(L t,L *e) {
 L d = *e,v = car(t),x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(car(d)))) d = cdr(d);
 return T(d) == CONS ? cell[ord(car(d))] = x : err(2);
}
L f_setcar(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a);
 return (T(x) == CONS) ? cell[ord(x)+1] = evarg(&t,e,&a) : err(1);
}
L f_setcdr(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a);
 return (T(x) == CONS) ? cell[ord(x)] = evarg(&t,e,&a) : err(1);
}
L f_macro(L t,L *_) { return macro(car(t),car(cdr(t))); }
L f_read(L t,L *_) { L x; char c = see; see = ' '; x = Read(); see = c; return x; }
L f_print(L t,L *e) { I a = 0; L x; while (!not(t)) print(evarg(&t,e,&a)); return nil; }
L f_println(L t,L *e) { f_print(t,e); putchar('\n'); return nil; }

/* section 12: adding readline with history */
L f_load(L t,L *e) { L x = car(t); if (!in && T(x) == ATOM) in = fopen(A+ord(x),"r"); return x; }

/* section 14: error handling and exceptions */
L f_catch(L t,L *e) {
 L x; I i;
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 i = setjmp(jb);
 x = i ? cons(atom("ERR"),i) : eval(car(t),*e);
 memcpy(jb,savedjb,sizeof(jb));
 return x;
}
L f_throw(L t,L *_) { longjmp(jb,(I)num(car(t))); }

struct { const char *s; L (*f)(L,L*); short t; } prim[] = {
 {"eval",    f_eval,   1},
 {"quote",   f_quote,  0},
 {"cons",    f_cons,   0},
 {"car",     f_car,    0},
 {"cdr",     f_cdr,    0},
 {"+",       f_add,    0},
 {"-",       f_sub,    0},
 {"*",       f_mul,    0},
 {"/",       f_div,    0},
 {"int",     f_int,    0},
 {"<",       f_lt,     0},
 {"eq?",     f_eq,     0},
 {"pair?",   f_pair,   0},
 {"or",      f_or,     0},
 {"and",     f_and,    0},
 {"not",     f_not,    0},
 {"cond",    f_cond,   1},
 {"if",      f_if,     1},
 {"let*",    f_leta,   1},
 {"lambda",  f_lambda, 0},
 {"define",  f_define, 0},
 {"assoc",   f_assoc,  0},
 {"env",     f_env,    0},
 {"let",     f_let,    1},
 {"letrec*", f_letreca,1},
 {"setq",    f_setq,   0},
 {"set-car!",f_setcar, 0},
 {"set-cdr!",f_setcdr, 0},
 {"macro",   f_macro,  0},
 {"read",    f_read,   0},
 {"print",   f_print,  0},
 {"println", f_println,0},
 {"load",    f_load,   0},
 {"catch",   f_catch,  0},
 {"throw",   f_throw,  0},
 {0}};

/* section 16.2/3/4: tail-call optimization */
L eval(L x,L e) {
 I a; L f,v,d;
 while (1) {
  if (T(x) == ATOM) return assoc(x,e);
  if (T(x) != CONS) return x;
  f = eval(car(x),e); x = cdr(x);
  if (T(f) == PRIM) {
   x = prim[ord(f)].f(x,&e);
   if (prim[ord(f)].t) continue;
   return x;
  }
  if (T(f) == MACR) {
   for (d = env,v = car(f); T(v) == CONS; v = cdr(v),x = cdr(x)) d = pair(car(v),car(x),d);
   x = eval(cdr(f),d);
   continue;
  }
  if (T(f) != CLOS) return err(3);
  v = car(car(f)); d = cdr(f);
  if (T(d) == NIL) d = env;
  for (a = 0; T(v) == CONS; v = cdr(v)) d = pair(car(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? x : evlis(x,e),d);
  x = cdr(car(f)); e = d;
 }
}

/* section 12: adding readline with history */
void look() {
 if (in) {
  int c = getc(in);
  see = c;
  if (c != EOF) return;
  fclose(in);
  in = NULL;
 }
 if (see == '\n') {
  if (line) { ptr = line; line = NULL; free(ptr); }
  while (!(ptr = line = readline(ps))) freopen("/dev/tty","r",stdin);
  add_history(line);
  strcpy(ps,"? ");
 }
 if (!(see = *ptr++)) see = '\n';
}
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }
char get() { char c = see; look(); return c; }

/* section 7: parsing Lisp expressions */
char scan() {
 I i = 0;
 while (seeing(' ') || seeing(';')) if (get() == ';') while (!seeing('\n')) get();
 if (seeing('(') || seeing(')') || seeing('\'')) buf[i++] = get();
 else do buf[i++] = get(); while (i < 39 && !seeing('(') && !seeing(')') && !seeing(' '));
 return buf[i] = 0,*buf;
}
L Read() { return scan(),parse(); }

/* section 16.1: replacing recursion with loops (in list parsing) */
L list() {
 L t = nil,*p = &t;
 while (1) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = Read(),scan(),t;
  *p = cons(parse(),nil),p = cell+sp;
 }
}
L parse() {
 L n; I i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(Read(),nil));
 return sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf);
}

/* section 8: printing Lisp expressions */
void printlist(L t) {
 putchar('(');
 while (1) {
  print(car(t));
  if (not(t = cdr(t))) break;
  if (T(t) != CONS) { printf(" . "); print(t); break; }
  putchar(' ');
 }
 putchar(')');
}
void print(L x) {
 if (T(x) == NIL) printf("()");
 else if (T(x) == ATOM) printf("%s",A+ord(x));
 else if (T(x) == PRIM) printf("<%s>",prim[ord(x)].s);
 else if (T(x) == CONS) printlist(x);
 else if (T(x) == CLOS) printf("{%u}",ord(x));
 else if (T(x) == MACR) printf("[%u]",ord(x));
 else printf("%.10lg",x);
}

/* section 9: garbage collection */
void gc() {
 I i;
 for (hp = 0,i = sp = ord(env); i < N; ++i) if (T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
}

/* section 14: error handling and exceptions */
void stop(int i) { if (line) longjmp(jb,5); else abort(); }

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; printf("tinylisp");
 nil = box(NIL,0); atom("nan"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 in = fopen((argc > 1 ? argv[1] : "common.lisp"),"r");
 using_history();
 if ((i = setjmp(jb)) > 0) printf("ERR %u",i);
 signal(SIGINT,stop);
 while (1) { putchar('\n'); snprintf(ps,20,"%u>",sp-hp/8); print(eval(Read(),env)); gc(); }
}
