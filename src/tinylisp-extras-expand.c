/* tinylisp-extras-expand.c with extras + expand hygienic macros by Robert A. van Engelen 2026 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* we only need two types to implement a Lisp interpreter:
        I      unsigned integer
        L      Lisp expression (floating point double with NaN boxing)
   I variables and function parameters are named as follows:
        i      any unsigned integer, e.g. a NaN-boxed ordinal value or index
        t      a NaN-boxed tag
        a      dot operator argument flag, used with evarg()
   L variables and function parameters are named as follows:
        x,y    any Lisp expression
        n      number
        t,s    list
        f,g    function, a lambda closure or Lisp primitive or macro
        p      pair, a cons of two Lisp expressions
        e,d,h  environment, a list of pairs, e.g. created with (define v x)
        b,c    macro argument bindings environment, used in expand()
        v,w    the name of a variable (an atom) or a list of variables */
#define I uint32_t
#define L double

/* address of the atom heap is at the bottom of the cell stack */
#define A (char*)cell

/* number of cells for the shared stack of cells and atom heap, increase N as desired */
#define N 65536

/* section 12: adding readline with history */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in = NULL;
char buf[256],see = 0,*ptr = "",*line = NULL,ps[80];

/* prompt strings for readline (truncates to 80 chars max), use \001 to ignore codes up to \002 */
/* NOTE: MacOS Darwin uses libedit as a libreadline "compatible", but that does not display prompt colors! */
#define PS1 "\001\e[32;1m\002%u>\001\e[m\002"
#define PS2 "\001\e[32;1m\002? \001\e[m\002"

/* forward proto declarations */
L eval(L,L),expand(L,L,L),Read(),parse(),err(I,L); void print(L);

/* section 4: constructing Lisp expressions */
/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   sp: cell stack pointer, the stack starts at the top of cell[] with sp=N
   tr: tracing off (0), on (1), wait on ENTER (2), dump and wait (3)
   xh: exception handler nesting depth
   safety invariant: hp <= sp<<3 */
I hp = 0,sp = N,tr = 0,xh = 0;
/* atom, primitive, cons, closure and nil tags for NaN boxing */
enum { ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd,HOLD = 0x7ffe };
/* cell[N] array of Lisp expressions, shared by the stack and atom heap */
L cell[N];
/* Lisp constant expressions () (nil), #t, and the global environment env */
L nil,tru,env;
/* section 17.1: early binding and efficient macro expansion */
L p_quote,p_lambda,p_macro,p_cond,p_leta,p_let,p_letreca,p_letrec;
/* NaN-boxing specific functions:
   T(x):     returns the tag bits of a NaN-boxed double x
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
I T(L x) { union { L x; uint64_t i; } u = {x}; return u.i>>48; }
L box(I t,I i) { union { uint64_t i; L x; } u = {(uint64_t)t<<48|i}; return u.x; }
I ord(L x) { union { L x; uint64_t i; } u = {x}; return u.i; }
L num(L n) { return n; }
I equ(L x,L y) { union { L x; uint64_t i; } u = {x},v = {y}; return u.i == v.i; }
/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 return i == hp && (hp += strlen(strcpy(A+i,s))+1) > sp<<3 ? err(4,nil) : box(ATOM,i);
}

/* section 14: error handling and exceptions
   ERR 1: not a pair
   ERR 2: unbound symbol
   ERR 3: cannot apply
   ERR 4: out of memory
   ERR 5: cannot open
   ERR 6: program stopped */
#include <setjmp.h>
#include <signal.h>
jmp_buf jb;
L err(I i,L x) {
 const char *msg[7] = {"not a pair","unbound","cannot apply","out of memory","cannot open","stopped","syntax"};
 if (!xh || tr) { printf("\n\e[31;1mERR %u: ",i); print(x); printf(" %s\e[m\n",i >= 1 && i <= 7 ? msg[i-1] : ""); }
 longjmp(jb,i);
}

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(L x,L y) { cell[--sp] = x; cell[--sp] = y; if (hp+16 > sp<<3) err(4,nil); return box(CONS,sp); }
/* return the car of a pair or throw err(1) if not a pair */
L car(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)+1] : err(1,p); }
/* return the cdr of a pair or throw err(1) if not a pair */
L cdr(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)] : err(1,p); }
/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
/* construct a lambda closure with variables v body x environment e, returns a NaN-boxed CLOS */
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,equ(e,env) ? nil : e))); }
/* construct a macro with variables v body x, returns a NaN-boxed MACR */
L macro(L v,L x) { return box(MACR,ord(cons(v,x))); }
/* look up a symbol v in environment e, return its value or throw err(2) if not found */
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e); return T(e) == CONS ? cdr(car(e)) : err(2,v); }
/* not(x) is nonzero if x is the Lisp () empty list a.k.a. nil or false */
I not(L x) { return T(x) == NIL; }
/* let(x) is nonzero if x has more than one item, used by let* */
I let(L x) { return !not(x) && !not(cdr(x)); }

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
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 x = car(*t); *t = cdr(*t);
 return *a ? x : eval(x,*e);
}
I isarg(L *t,L *e,I *a,L *x) {
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 if (not(*t)) return 0;
 *x = car(*t); *t = cdr(*t);
 if (!*a) *x = eval(*x,*e);
 return 1;
}

/* section 6 Lisp primitives (optimized with evarg per section 16.4) */
L f_eval(L t,L *e) { I a = 0; return evarg(&t,e,&a); }
L f_quote(L t,L *_) { return car(t); }
L f_cons(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return cons(x,evarg(&t,e,&a)); }
L f_car(L t,L *e) { I a = 0; return car(evarg(&t,e,&a)); }
L f_cdr(L t,L *e) { I a = 0; return cdr(evarg(&t,e,&a)); }
L f_add(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n += x; return num(n); }
L f_sub(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n -= x; return num(n); }
L f_mul(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n *= x; return num(n); }
L f_div(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n /= x; return num(n); }
L f_int(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n < 1e16 && n > -1e16 ? (long long)n : n; }
L f_lt(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n - evarg(&t,e,&a) < 0 ? tru : nil; }
L f_eq(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return equ(x,evarg(&t,e,&a)) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return T(x) == CONS ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; while (isarg(&t,e,&a,&x) && not(x)) continue; return x; }
L f_and(L t,L *e) { I a = 0; L x = tru; while (isarg(&t,e,&a,&x) && !not(x)) continue; return x; }
L f_not(L t,L *e) { I a = 0; return not(evarg(&t,e,&a)) ? tru : nil; }
L f_cond(L t,L *e) { while (not(eval(car(car(t)),*e))) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { return car(cdr(not(eval(car(t),*e)) ? cdr(t) : t)); }
L f_leta(L t,L *e) { for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),*e),*e); return car(t); }
L f_lambda(L t,L *e) { return closure(car(t),car(cdr(t)),*e); }
/* section 17.1-2: early binding and efficient macro expansion with hygienic macros */
L f_define(L t,L *e) {
 L d = *e,v = car(t);
 if (T(v) == PRIM) printf("not redefined built-in ");
 else if (T(v) != ATOM) {
  if (T(v) != CLOS && T(v) != MACR) return err(2,v);
  while (T(d) == CONS && !equ(v,cdr(car(d)))) d = cdr(d);
  if (T(d) != CONS) return err(2,v);
  v = car(car(d));
 }
 env = pair(v,eval(car(cdr(t)),*e),env);
 return v;
}

/* section 11: additional Lisp primitives (optimized with evarg() see section 16.4) */
L f_assoc(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return assoc(x,evarg(&t,e,&a)); }
L f_env(L _,L *e) { return *e; }
L f_let(L t,L *e) {
 L d = *e;
 for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),d),*e);
 return car(t);
}
L f_letreca(L t,L *e) {
 for (; let(t); t = cdr(t)) *e = pair(car(car(t)),nil,*e),cell[ord(car(*e))] = eval(car(cdr(car(t))),*e);
 return car(t);
}
L f_letrec(L t,L *e) {
 L s,d,*p;
 for (s = t,d = *e,p = &d; let(s); s = cdr(s),p = &cell[ord(*p)]) *p = pair(car(car(s)),nil,*e);
 for (*e = d; let(t); t = cdr(t),d = cdr(d)) cell[ord(car(d))] = eval(car(cdr(car(t))),*e);
 return car(t);
}
L f_setq(L t,L *e) {
 L d = *e,v = car(t),x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(car(d)))) d = cdr(d);
 return T(d) == CONS ? cell[ord(car(d))] = x : err(2,v);
}
L f_setcar(L t,L *e) {
 I a = 0; L p = evarg(&t,e,&a);
 return (T(p) == CONS) ? cell[ord(p)+1] = evarg(&t,e,&a) : err(1,p);
}
L f_setcdr(L t,L *e) {
 I a = 0; L p = evarg(&t,e,&a);
 return (T(p) == CONS) ? cell[ord(p)] = evarg(&t,e,&a) : err(1,p);
}
L f_macro(L t,L *_) { return macro(car(t),car(cdr(t))); }
L f_read(L t,L *_) { L x; char c = see; see = 0; x = Read(); see = c; return x; }
L f_print(L t,L *e) { I a = 0; L x; while (isarg(&t,e,&a,&x)) print(x); return nil; }
L f_println(L t,L *e) { f_print(t,e); putchar('\n'); return nil; }

/* section 12: adding readline with history */
L f_load(L t,L *_) { L x = car(t); return !in && T(x) == ATOM && (in = fopen(A+ord(x),"r")) ? x : err(5,x); }

/* section 13: execution tracing */
L f_trace(L t,L *_) { tr = not(t) ? !tr : (I)num(car(t)); return num(tr); }

/* section 14: error handling and exceptions */
L f_catch(L t,L *e) {
 L x; I i;
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 ++xh;
 if ((i = setjmp(jb)) == 0) x = eval(car(t),*e);
 --xh;
 memcpy(jb,savedjb,sizeof(jb));
 return i == 0 ? x : i == 4 || i == 6 ? err(i,nil) : cons(atom("ERR"),i);
}
L f_throw(L t,L *_) { longjmp(jb,(I)num(car(t))); }

/* section 16.5: tail-call optimization */
L f_progn(L t,L *e) {
 for (; let(t); t = cdr(t)) eval(car(t),*e);
 return car(t);
}
L f_while(L t,L *e) {
 L s,x = nil;
 while (!not(eval(car(t),*e)))
  for (s = cdr(t); T(s) == CONS; s = cdr(s)) x = eval(car(s),*e);
 return x;
}
L f_until(L t,L *e) {
 L s,x = nil;
 do
  for (s = t; T(s) == CONS; s = cdr(s)) x = eval(car(s),*e);
 while (not(x));
 return x;
}

/* (list ...) returns a list of its arguments (e.g. used in backquoting) */
L f_list(L t,L *e) { return evlis(t,*e); }

/* (append ...) returns the concatenation of its list arguments as a new list (e.g. used in backquoting) */
L f_append(L t,L *e) {
 I a = 0; L x = nil,s,*p = &s;
 while (isarg(&t,e,&a,&x) && !not(t))
  for (; !not(x); x = cdr(x),p = cell+sp) *p = cons(car(x),nil);
 *p = x;
 return s;
}

L f_quit(L t,L *e) { I a = 0; L x; exit(isarg(&t,e,&a,&x) ? (int)num(x) : 0); }

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
 {"letrec" , f_letrec, 1},
 {"setq",    f_setq,   0},
 {"set-car!",f_setcar, 0},
 {"set-cdr!",f_setcdr, 0},
 {"macro",   f_macro,  0},
 {"read",    f_read,   0},
 {"print",   f_print,  0},
 {"println", f_println,0},
 {"load",    f_load,   0},
 {"trace",   f_trace,  0},
 {"catch",   f_catch,  0},
 {"throw",   f_throw,  0},
 {"progn",   f_progn,  1},
 {"while",   f_while,  0},
 {"until",   f_until,  0},
 {"list",    f_list,   0},
 {"append",  f_append, 0},
 {"quit",    f_quit,   0},
 {0}};

/* section 13: tracing (trace 1) with colorful output, to wait on ENTER (trace 2), with memory dump (trace 3) */
void dump(I i,I k,L e) {
 if (i < k) {
  printf("\n\e[35m==== DUMP ====");
  while (i < k--) {
   printf("\n\e[35m%s\e[32m%u \e[35m",k-1 == ord(e) ? "local env:\n" : k-1 == ord(env) ? "env:\n" : "",k);
   switch (T(cell[k])) {
    case ATOM: printf("ATOM "); printf("\e[32m%u ",ord(cell[k])); break;
    case PRIM: printf("PRIM "); break;
    case CONS: printf("CONS "); printf("\e[32m%u ",ord(cell[k])); break;
    case CLOS: printf("CLOS "); printf("\e[32m%u ",ord(cell[k])); break;
    case MACR: printf("MACR "); printf("\e[32m%u ",ord(cell[k])); break;
    case NIL:  printf("NIL  "); break;
    default:   printf("     "); break;
   }
   printf("\e[33m"); print(cell[k]); printf("\e[m%s",k % 2 ? "" : "\n");
  }
  printf("\e[35m==============\e[m\t");
 }
}
void trace(I s,L y,L x,L e) {
 if (tr > 2) dump(sp,s,e);
 printf("\n\e[32m%u \e[33m",sp); print(y); printf("\e[36m => \e[33m"); print(x); printf("\e[m\t");
 if (tr > 1) while (getchar() >= ' ') continue;
}

/* section 16.5: tail-call optimization (to overwrite closure arguments) */
void assign(L v,L x,L e) { while (!equ(v,car(car(e)))) e = cdr(e); cell[ord(car(e))] = x; }

/* section 16.2-5: tail-call optimization (section 17.2: hygienic macros - remove MACR branch) */
L eval(L x,L e) {
 I a,s = sp; L f,v,d,y,g = nil,h = nil;
 while (1) {
  /* copy x to y to output y => x when tracing is enabled */
  y = x;
  /* if ix is an atom, then return its value; if x is not an application list (it is constant), then return x */
  if (T(x) == ATOM) { x = assoc(x,e); break; }
  if (T(x) != CONS) break;
  /* evaluate f in the application (f . x) and get the list of arguments x */
  f = eval(car(x),e); x = cdr(x);
  if (T(f) == PRIM) {
   /* apply Lisp primitive to argument list x, return value in x */
   x = prim[ord(f)].f(x,&e);
   /* if tail-call then continue evaluating x, otherwise return x */
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) != CLOS) return err(3,f);
  /* get the list of variables v of closure f */
  v = car(car(f));
  /* if closure f is tail-recursive, then we update its previous environment e, otherwise obey static scoping */
  if (equ(f,g)) d = e;
  else if (not(d = cdr(f))) d = env;
  /* bind closure f variables v to the evaluated argument values */
  for (a = 0; T(v) == CONS; v = cdr(v)) d = pair(car(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? x : evlis(x,e),d);
  if (equ(f,g)) {
   /* reassign tail-recursive closure f vars in e to new values, delete bindings, but don't delete other stacked data */
   for (; !equ(d,e) && sp == ord(d); d = cdr(d),sp += 4) assign(car(car(d)),cdr(car(d)),e);
   /* delete let-locals of tail-recursive closure f, but don't delete other stacked data */
   for (; !equ(d,h) && sp == ord(d); d = cdr(d)) sp += 4;
  }
  /* next, evaluate body x of closure f in environment e = d, track tail-recursive closures g = f, also save h = e */
  x = cdr(car(f)); e = d; g = f; h = e;
  if (tr) trace(s,y,x,e);
 }
 if (tr && !equ(x,y)) trace(s,y,x,e);
 return x;
}

/* section 12: adding readline with history */
void look() {
 if (in) {
  int c = getc(in);
  see = c;
  if (c != EOF) return;
  fclose(in);
  in = NULL;
  see = 0;
 }
 if (!see) {
  if (line) { ptr = line; line = NULL; free(ptr); }
  while (!(ptr = line = readline(ps))) freopen("/dev/tty","r",stdin);
  add_history(line);
  snprintf(ps,sizeof(ps),PS2);
 }
 see = *ptr++;
}
I seeing(char c) { return c == ' ' ? see >= 0 && see <= c : (c == '\n' && !see) || see == c; }
char get() { char c = see; look(); return c; }

/* section 7: parsing Lisp expressions */
char scan() {
 I i = 0;
 while (seeing(' ') || seeing(';')) if (get() == ';') while (!seeing('\n')) get();
 if (seeing('(') || seeing(')') || seeing('\'') || seeing('`') || seeing(',')) buf[i++] = get();
 else if (seeing('"')) do buf[i++] = get(); while (i < sizeof(buf)-1 && (!seeing('"') || !get()));
 else do buf[i++] = get(); while (i < sizeof(buf)-1 && !seeing('(') && !seeing(')') && !seeing(' '));
 return buf[i] = 0,*buf;
}
L Read() { return scan(),parse(); }

/* section 16.1: replacing recursion with loops (in list parsing) */
L quote(L x) { return cons(atom("quote"),cons(x,nil)); }        /* returns (quote x) */
L endl(L t) { return scan() == ')' ? t : err(7,t); }            /* err 7 when closing ) is missing */
L list() {
 L t,*p;
 for (t = nil,p = &t; ; *p = cons(parse(),nil),p = cell+sp) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = Read(),endl(t);
 }
}
L tick() {
 L t,*p;
 if (*buf == ',') return Read();
 if (*buf == '\'') return scan(),cons(atom("list"),cons(quote(atom("quote")),cons(tick(),nil)));
 if (*buf == '"') return parse();
 if (*buf == ')') return err(7,atom(buf));
 if (*buf != '(') return quote(parse());
 for (t = cons(atom("list"),nil),p = cell+sp; ; *p = cons(tick(),nil),p = cell+sp) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return scan(),endl(cons(atom("append"),cons(t,cons(tick(),nil))));
 }
}
L parse() {
 L n; int i;
 if (*buf == '(') return list();
 if (*buf == '\'') return quote(Read());
 if (*buf == '`') return scan(),tick();
 if (*buf == '"') return quote(atom(buf+1));
 if (*buf == ',') return err(7,atom(buf));
 if (*buf == ')') return err(7,atom(buf));
 return sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf);
}

/* section 17.1: early binding and efficient macro expansion */
/* look up variable v in environment e, return 1 when found and x is set to its value, otherwise return 0 */
I lookup(L v,L e,L *x) {
 while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e);
 if (T(e) != CONS) return 0;
 *x = cdr(car(e));
 return 1;
}
/* section 17.2: hygienic macros */
/* return a copy of expression of x that holds all variables (atoms) by placing each ATOM in a HOLD */
L hold(L x) {
 if (T(x) == ATOM) return box(HOLD,ord(x));
 if (T(x) == CONS) {
  L t = nil,*p = &t;
  for (; T(x) == CONS; x = cdr(x),p = cell+sp) *p = cons(hold(car(x)),nil);
  *p = hold(x);
  return t;
 }
 return x;
}
/* check if expression x has variable (atom) v placed on HOLD */
I holds(L v,L x) {
 if (T(x) == HOLD) return ord(v) == ord(x);
 if (T(x) == CONS) return holds(v,car(x)) || holds(v,cdr(x));
 return 0;
}
/* generate a new symbol for atom v by prepending an underscore _ to the symbol v */
L gensym(L v) { char sym[sizeof(buf)]; snprintf(sym,sizeof(sym),"_%s",A+ord(v)); return atom(sym); }
/* if necessary rename variable(s) v to prevent "variable shadowing" in macro-expanded expression x */
L hygienic(L v,L x) {
 if (T(v) == CONS) {
  L w = nil,*p = &w;
  for (; T(v) == CONS; v = cdr(v),p = cell+sp) *p = cons(hygienic(car(v),x),nil);
  if (T(v) == ATOM || T(v) == HOLD) *p = hygienic(v,x);
  return w;
 }
 if (T(v) == HOLD) return box(ATOM,ord(v));
 if (T(v) == ATOM) while (holds(v,x)) v = gensym(v);
 return v;
}
/* release all variables held in x; this operation destructively replaces each HOLD by ATOM throughout x */
L release(L x) {
 if (T(x) == HOLD) return box(ATOM,ord(x));
 if (T(x) == CONS) {
  L *p = &x;
  for (; T(*p) == CONS; p = &cell[ord(*p)]) cell[ord(*p)+1] = release(car(*p));
  *p = release(*p);
 }
 return x;
}
/* section 17.1-2: early binding and efficient macro expansion with hygienic macros */
L expand(L x,L e,L b) {
 L c,d,v,w,y;
 if (T(x) == ATOM) {
  /* resolve the name of a variable (atom) x */
  if (lookup(x,b,&y)) return y;         /* x is a macro argument in a macro body */
  if (lookup(x,e,&y) && (T(y) == PRIM || T(y) == CLOS || T(y) == MACR)) return y;
  return x;
 }
 if (T(x) == HOLD) {
  x = release(x);                       /* release variable (atom) x being held */
  if (lookup(x,e,&y) && (T(y) == PRIM || T(y) == CLOS || T(y) == MACR)) return y;
  return x;
 }
 if (T(x) == CLOS) {
  /* expand the body of closure x */
  v = w = car(car(x));                  /* the variables v of the closure */
  d = e = cdr(x);                       /* the lexical scope of bindings d of the closure */
  y = cdr(car(x));                      /* the body y of the closure */
  if (T(d) == NIL) d = env;             /* closure has global scope */
  /* closure variables v hide macro variables in b and hide global primitives and macros in d */
  for (c = b; T(v) == CONS; v = cdr(v)) d = pair(car(v),nil,d),c = pair(car(v),car(v),c);
  if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,v,c);
  /* expand the body y of the closure and create a new closure with variables w and lexical scope e */
  return closure(w,expand(y,d,c),e);
 }
 if (T(x) == CONS) {
  /* expand the application x = (f ...) in which we first expand f */
  L f = expand(car(x),e,b);
  /* then we construct a new application list t = (f ...) by populating *p = ... with the list of expanded arguments */
  L t = cons(f,nil),*p = cell+sp;
  x = cdr(x);
  if (T(f) == MACR) {
   /* f in (f ...) is a macro to apply by expand/eval/expand its body */
   I i; jmp_buf savedjb;
   memcpy(savedjb,jb,sizeof(jb));
   /* bind the variables v of macro f to the given arguments x quoted (and hold all atoms in x) in environment c */
   for (c = nil,v = release(car(f)); T(v) == CONS; v = cdr(v),x = cdr(x)) c = pair(car(v),quote(hold(car(x))),c);
   if (T(v) == ATOM) c = pair(v,quote(hold(x)),c);
   /* expand macro body cdr(f) using macro arguments bound in updated environment c */
   x = expand(cdr(f),e,c);
   /* eval macro body (may fail) then expand the result with macro arguments bound in environment b */
   if ((i = setjmp(jb)) == 0) y = expand(eval(x,e),e,b);
   memcpy(jb,savedjb,sizeof(jb));
   if (i) { printf("\e[31;1mmacro expansion failed:\e[m "); print(x); printf("\n"); longjmp(jb,i); }
   return y;
  }
  if (T(f) == PRIM) {
   /* f is a primitive in (f ...) */
   if (equ(f,p_quote)) { *p = release(x); return t; }
   if (equ(f,p_macro)) { *p = cons(car(x),cons(expand(car(cdr(x)),e,b),nil)); return t; }
   if (equ(f,p_lambda)) {
    /* <lambda> arguments v hide macro variables in b and hide global primitives and macros in e */
    v = car(x); w = hygienic(v,cdr(x));
    *p = cons(w,nil);
    for (d = e,c = b; T(v) == CONS; v = cdr(v),w = cdr(w))
     if (T(car(v)) == ATOM) d = pair(car(v),nil,d),c = pair(car(v),car(w),c);
    if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    cell[ord(*p)] = cons(expand(car(cdr(x)),d,c),nil);  /* expand <lambda> body */
    return t;
   }
   if (equ(f,p_cond)) {
    /* <cond> arguments are pairs of expressions (each cons pair is not a function application) */
    for (; T(x) == CONS; x = cdr(x),p = cell+sp)
     *p = cons(cons(expand(car(car(x)),e,b),cons(expand(car(cdr(car(x))),e,b),nil)),nil);
    return t;
   }
   if (equ(f,p_leta)) {
    /* <let*> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = cdr(x),p = &cell[ord(*p)]) {
     v = car(car(x)); w = hygienic(v,cdr(x));
     *p = cons(cons(w,cons(expand(car(cdr(car(x))),d,c),nil)),nil);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <let*> body */
    return t;
   }
   if (equ(f,p_let)) {
    /* <let> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = cdr(x),p = &cell[ord(*p)]) {
     v = car(car(x)); w = hygienic(v,cdr(x));
     *p = cons(cons(w,cons(expand(car(cdr(car(x))),e,c),nil)),nil);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <let> body */
    return t;
   }
   if (equ(f,p_letreca)) {
    /* <letrec*> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = cdr(x),p = &cell[ord(*p)]) {
     v = car(car(x)); w = hygienic(v,x);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
     *p = cons(cons(w,cons(expand(car(cdr(car(x))),d,c),nil)),nil);
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <letrec*> body */
    return t;
   }
   if (equ(f,p_letrec)) {
    /* <letrec> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b,y = x; let(y); y = cdr(y)) {
     v = car(car(y)); w = hygienic(v,x);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    for (y = x; let(y); y = cdr(y),p = &cell[ord(*p)]) {
     v = car(car(y)); w = hygienic(v,x);
     *p = cons(cons(w,cons(expand(car(cdr(car(y))),d,c),nil)),nil);
    }
    *p = cons(expand(car(y),d,c),nil);                  /* expand <letrec> body */
    return t;
   }
  }
  /* expand argument expressions x in (f . x) and return a new application t = (f ...) by populating *p */
  for (; T(x) == CONS; x = cdr(x),p = cell+sp) *p = cons(expand(car(x),e,b),nil);
  *p = expand(x,e,b);
  return t;
 }
 return x;
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
 else if (T(x) == HOLD) printf("|%s|",A+ord(x));
 else printf("%.10lg",x);
}

/* section 9: garbage collection */
void gc() {
 I i = sp = ord(env);
 for (hp = 0; i < N; ++i) if (T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
}

/* section 14: error handling and exceptions */
void stop(int i) { if (line) err(6,nil); else abort(); }

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; printf("tinylisp-extras-expand");
 nil = box(NIL,0); atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 /* section 17.1: early binding and efficient macro expansion */
 p_quote   = assoc(atom("quote"),env);
 p_lambda  = assoc(atom("lambda"),env);
 p_macro   = assoc(atom("macro"),env);
 p_cond    = assoc(atom("cond"),env);
 p_leta    = assoc(atom("let*"),env);
 p_let     = assoc(atom("let"),env);
 p_letreca = assoc(atom("letrec*"),env);
 p_letrec  = assoc(atom("letrec"),env);
 if (!(in = fopen((argc > 1 ? argv[1] : "common.lisp"),"r"))) printf("\nERR 5");
 using_history();
 signal(SIGINT,stop);
 if ((i = setjmp(jb)) > 0) { printf("ERR %u",i); if (i == 7) see = 0; }
 /* section 17.1: early binding and efficient macro expansion (REEPL = REPL with expand) */
 while (1) { gc(); putchar('\n'); snprintf(ps,sizeof(ps),PS1,sp-hp/8); print(eval(expand(Read(),env,nil),env)); }
}
