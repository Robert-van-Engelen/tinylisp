/* tinylisp-extras-expand-ms.c more extras + expand hygienic macros + mark-sweep GC by Robert A. van Engelen 2026 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h> /* to return NAN from num() */

/* MS=0: mark-sweep only when no free cell space remains, but allocating new atom symbols may fail with ERR 4 */
/* MS=1: mark-sweep when the remaining free cell space halves, i.e. when 1/2 or 1/4 or 1/8 ... space remains */
/* MS=2: like MS=1, but avoid triggering repeated mark-sweep when ping-pong around 1/2, 1/4, 1/8, ... thresholds */
/* MS=3: mark-sweep continuously to battle-test garbage collector API calls (this is slow!) */
#ifndef MS
# define MS 0
#endif

/* we only need two types to implement a Lisp interpreter:
        I      unsigned integer
        L      Lisp expression (floating point double with NaN boxing)
   I variables and function parameters are named as follows:
        i,j,k  any unsigned integer, e.g. a NaN-boxed ordinal value i or an index i or a counter k
        t      a NaN-boxed tag
        a      dot operator argument flag, used with evarg()
   L variables and function parameters are named as follows:
        x,y,z  any Lisp expression
        n      number
        t,s    list
        f,g    function, a lambda closure or Lisp primitive or macro
        p      pair, a cons of two Lisp expressions
        e,d,h  environment, a list of pairs, e.g. created with (define v x)
        b,c    macro argument bindings environment, used in expand()
        v,w    the name of a variable (an atom) or a list of variables */
#define I uint32_t
#define L double

/* address of the atom heap is at the bottom of the cell pool */
#define A (char*)cell

/* number of cells for the shared pool and atom heap, increase N as desired */
#define N 8192

/* section 12: adding readline with history ++ new: support nested load, new err 5 can't open file */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in[10],*out;
char buf[256],see = 0,*ptr = "",*line = NULL,ps[80];

/* prompt strings for readline (truncates to 80 chars max), use \001 to ignore codes up to \002 */
/* NOTE: MacOS Darwin uses libedit as a libreadline "compatible", but that does not display prompt colors! */
#define PS1 "\001\e[32;1m\002%u>\001\e[m\002"
#define PS2 "\001\e[32;1m\002? \001\e[m\002"

/* forward proto declarations */
L eval(L,L),expand(L,L,L),cede(L),Read(),parse(),err(I,L); void ms(L),print(FILE*,L),stop(int); I atomize(L,char*);

/* section 4: constructing Lisp expressions (using a cell pool managed with reference count garbage collection) */
/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   fp: free cell pairs list pointer, cell[fp] is the head of the list of free cell pairs
   lp: pointer to the lowest allocated and used cell pair in cell[]
   fn: number of free cell cons pairs, not taking space used by atoms into account (for reporting only, not required)
   fl: set to fn by the last mark-sweep performed, to avoid excessive mark-sweep when ping-pong around thesholds
   tr: tracing off (0), on (1), wait on ENTER (2), dump and wait (3)
   ld: number of open loads from input files (nested load up to 10 levels deep)
   safety invariant: hp < (lp-2)<<3 */
I hp = 0,fp = N-2,lp = N-2,fn = N/2,fl = N/2,tr = 0,ld = 0;
/* atom, primitive, cons, closure and nil tags for NaN boxing */
enum { ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd,HOLD = 0x7ffe };
/* cell[N] pool of allocatable Lisp expressions shared by the atom heap */
L cell[N];
/* bits[] bit array for marking used cell pairs in mark-sweep garbage collection */
I bits[((N/2)+31)/32];
/* check if the cell pair (cell[i],cell[i+1]) is used and to mark it as used */
I used(I i) { return bits[i/64]&(1<<i/2%32); }
void mark(I i) { bits[i/64] |= 1<<i/2%32; }
/* Lisp global environment env */
L env;
/* section 17.1: early binding and efficient macro expansion */
L p_quote,p_lambda,p_macro,p_cond,p_leta,p_let,p_letreca,p_letrec,p_define;
/* NaN-boxing specific functions:
   T(x):     returns the tag bits of a NaN-boxed double x
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   check number, return NAN = ERR = box(ATOM,0) when not a number
   equ(x,y): returns nonzero if x equals y */
I T(L x) { union { L x; uint64_t i; } u = {x}; return u.i>>48; }
L box(I t,I i) { union { uint64_t i; L x; } u = {(uint64_t)t<<48|i}; return u.x; }
I ord(L x) { union { L x; uint64_t i; } u = {x}; return u.i; }
L num(L n) { return n == n ? n : NAN; }
I equ(L x,L y) { union { L x; uint64_t i; } u = {x},v = {y}; return u.i == v.i; }
/* Lisp constant expressions () (nil is false), ERR (same as NAN), and #t (true) */
#define nil box(NIL,0)          /* fixed constant, instead of nil = box(NIL,0) in main() */
#define ERR box(ATOM,0)         /* fixed constant, instead of ERR = atom("ERR") in main() */
#define tru box(ATOM,4)         /* fixed constant, instead of tru = atom("#t") in main() */
/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s) {
 I i = 0;
 while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp) {
  if ((hp += strlen(s)+1)+16 > lp<<3) ms(nil);
  memmove(A+i,s,hp-i);
 }
 return box(ATOM,i);
}

/* ++ new: mark-sweep garbage collector registry stack size S, max depth of nested calls to eval() = S/3 */
#define S 4096
/* mark-sweep garbage collector roots stack, stack pointer, and catch exception pointer */
L *stk[S],**sp,**xp;
/* lowest pointer to allocated cells in memory */
I lomem(I i) { return lp = i < lp ? i : lp; }
/* allocate and construct a new pair (x . y), returns a NaN-boxed CONS */
L cons(L x,L y) {
 I i = fp; L p = box(CONS,i);
 fp = ord(cell[i]); --fn; cell[i+1] = x; cell[i] = y;
 if ((MS == 1 && !(fn&(fn+1))) || (MS == 2 && !(fn&(fn+1)) && fn != fl) || MS > 2 || hp+16 > fp<<3) ms(p); else lomem(i);
 return p;
}
/* delete the pair cell[i] cell[i+1] to reuse by adding it to the free cell pair list */
void del(I i) { cell[i] = box(CONS,fp); fp = i; ++fn; }
/* register x as a root on the stack with initial value y to protect it from collected as garbage */
L rc(L *x,L y) { *x = y; *sp = x,++sp; return y; }      /* GCC incorrectly warns about *sp++ = x dangling pointer */
/* remove k registrations from the stack and return x */
L rr(I k,L x) { sp -= k; return x; }
/* ++ new: mark-sweep collector marking stage: recursively mark all cell pairs reachable from cell pair x */
void mk(L x) {
 I i; L y;
 while (!used(i = ord(x))) {                            /* repeat until all reachable cell pairs are marked */
  mark(i); x = cell[i]; y = cell[i+1];                  /* mark cell pair x, recurse on y = car(x) and x = cdr(x) */
  if (T(y) != CONS && T(y) != CLOS && T(y) != MACR) {
   if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) break;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) x = y;
  else mk(y);
 }
}
/* ++ new: mark-sweep garbage collector, releases unreachable cell pairs */
void ms(L p) {
 I i; L **q; fl = fn;                                   /* set fl to fn for MS=2 to avoid excessive ms() calls */
 signal(SIGINT,SIG_IGN);                                /* disable SIGINT CTRL-C: don't interrupt mark-sweep */
 memset(bits,0,sizeof(bits));
 if (T(p) == CONS) mk(p);                               /* mark root p as used */
 if (T(env) == CONS) mk(env);                           /* mark root env, recursively marks env cells as used */
 for (q = stk; q < sp; ++q)                             /* mark stack roots, marks registered cells as used */
  if (T(**q) == CONS || T(**q) == CLOS || T(**q) == MACR) mk(**q);
 for (hp = 0,i = 0; i < N; ++i)                         /* shrink the atom heap when possible */
  if (used(i) && (T(cell[i]) == ATOM || T(cell[i]) == HOLD) && ord(cell[i]) > hp) hp = ord(cell[i]);
 if (hp) hp += strlen(A+hp)+1;
 for (fp = 0,lp = N-2,fn = 1,i = (hp+31)/8&~1; i < N; i += 2)
  if (used(i)) lomem(i); else del(i);                   /* set lomem or add unused cells to the free list */
 signal(SIGINT,stop);                                   /* re-enable SIGINT CTRL-C */
 if (hp+16 > lp<<3) err(4,nil);                         /* if heap starts to overlap with lomem then ERR 4 */
}
/* clear stack and free up unused cell memory */
void gc() { sp = xp = stk; ms(nil); }

/* section 14: error handling and exceptions
   ERR 1: not a pair
   ERR 2: unbound symbol
   ERR 3: cannot apply
   ERR 4: out of memory
   ERR 5: cannot open file
   ERR 6: program stopped
   ERR 7: syntax error
   ERR 8: too few arguments */
#include <setjmp.h>
#include <signal.h>
jmp_buf jb;
/* report an error message when tracing or if error 1<=i<=8 without a catch handler */
void msg(I i,L x) {
 if (xp != stk ? tr : i >= 1 && i <= 8) {
  const char *s[8] = {"not a pair","unbound","cannot apply","out of memory","cannot open","stopped","syntax","few arg"};
  printf("\n\e[31;1mERR %u: ",i); print(stdout,x); printf(" %s\e[m\n",i >= 1 && i <= 8 ? s[i-1] : "");
 }
}
/* throw an error */
L err(I i,L x) { msg(i,x); longjmp(jb,i); }
/* SIGINT CTRL-C break running programs */
void stop(int i) { if (line) err(6,nil); else abort(); }

/* unsafe fast car and cdr, must be guarded to use: if (T(x) == CONS) { ... CAR(x) ... CDR(x) ... } */
#define CAR(p) cell[ord(p)+1]
#define CDR(p) cell[ord(p)]

/* return the car of a pair or throw err(1) if not a pair */
L car(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? CAR(p) : err(1,p); }
/* return the cdr of a pair or throw err(1) if not a pair */
L cdr(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? CDR(p) : err(1,p); }
/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
/* construct a closure, returns a NaN-boxed CLOS */
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,e))); }
/* construct a macro, returns a NaN-boxed MACR */
L macro(L v,L x) { return box(MACR,ord(cons(v,x))); }
/* look up a symbol in an environment, return its value or throw err(2) if not found */
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(CAR(e)))) e = CDR(e); return T(e) == CONS ? cdr(CAR(e)) : err(2,v); }
/* not(x) is nonzero if x is the Lisp () empty list */
I not(L x) { return T(x) == NIL; }
/* let(t) is nonzero if t has more than one list item */
I let(L x) { return T(x) == CONS && T(CDR(x)) == CONS; }
/* ++ new: opt(t) returns the first list item or (), i.e. the list t and the first item are optional */
L opt(L t) { return let(t) ? CAR(CDR(t)) : nil; }

/* section 16.1: replacing recursion with loops */
L evlis(L t,L e) {
 L s,*p = &s;
 for (rc(p,nil); T(t) == CONS; p = &CDR(*p),t = CDR(t)) *p = cons(eval(CAR(t),e),nil);
 if (T(t) == ATOM) *p = assoc(t,e);
 return rr(1,s);
}

/* section 16.4: optimizing the lisp primitives */
L evarg(L *t,L *e,I *a) {
 L x;
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 if (T(*t) != CONS) return err(8,nil);
 x = CAR(*t); *t = CDR(*t);
 return *a ? x : eval(x,*e);
}
I isarg(L *t,L *e,I *a,L *x) {
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 if (T(*t) != CONS) return 0;
 *x = CAR(*t); *t = CDR(*t);
 *x = *a ? *x : eval(*x,*e);
 return 1;
}

/* section 6 lisp primitives (optimized with evarg per section 16.4) */
L f_eval(L t,L *e) { I a = 0; return evarg(&t,e,&a); }
L f_quote(L t,L *_) { return car(t); }
L f_cons(L t,L *e) { I a = 0; L x,p; rc(&x,evarg(&t,e,&a)); p = cons(x,evarg(&t,e,&a)); return rr(1,p); }
L f_car(L t,L *e) { I a = 0; return car(evarg(&t,e,&a)); }
L f_cdr(L t,L *e) { I a = 0; return cdr(evarg(&t,e,&a)); }
L f_add(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n += x; return num(n); }
L f_sub(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n -= x; return num(n); }
L f_mul(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n *= x; return num(n); }
L f_div(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n /= x; return num(n); }
L f_int(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n < 1e16 && n > -1e16 ? (int64_t)n : num(n); }
/* ++ updated: (< x y [z ...]) returns #t if x < y and y < z ... etc when given, otherwise returns () */
I lt(L x,L y) {
 return (T(x) == ATOM && T(y) == ATOM ? strcmp(A+ord(x),A+ord(y)) < 0 :
     x == x && y == y ? x < y :
     T(x) < T(y) || (T(x) == T(y) && ord(x) < ord(y)));
}
L f_lt(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a),y;
 while (isarg(&t,e,&a,&y)) if (lt(x,y)) x = y; else return nil;
 return tru;
}
L f_eq(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return equ(cede(x),cede(evarg(&t,e,&a))) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return T(x) == CONS ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; while (isarg(&t,e,&a,&x) && not(x)) continue; return x; }
L f_and(L t,L *e) { I a = 0; L x = tru; while (isarg(&t,e,&a,&x) && !not(x)) continue; return x; }
L f_not(L t,L *e) { I a = 0; return not(evarg(&t,e,&a)) ? tru : nil; }
L f_cond(L t,L *e) { while (not(eval(car(car(t)),*e))) t = cdr(t); return opt(car(t)); }
L f_if(L t,L *e) { return opt(not(eval(car(t),*e)) ? cdr(t) : t); }
L f_leta(L t,L *e) {
 for (; let(t); t = CDR(t))
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),eval(opt(CAR(t)),*e),*e);
  else err(2,CAR(t));                           /* bound variable must be an atom */
 return car(t);
}
L f_lambda(L t,L *e) { return closure(car(t),opt(t),equ(*e,env) ? nil : *e); }
/* section 17.1-2: early binding and efficient macro expansion with hygienic macros */
/* ++ new: make old definitions unreachable when redefined, to garbage collect them */
L f_define(L t,L *e) {
 L d = env,v = car(t);
 if (T(v) == PRIM) printf("not redefined built-in ");
 else if (T(v) != ATOM && T(v) != CLOS && T(v) != MACR) return err(2,v);
 else {
  L x = eval(opt(t),*e);
  if (T(v) == CLOS || T(v) == MACR) {
   if (T(x) != T(v)) { printf("cannot redefine "); return v; }
   CAR(v) = CAR(x); CDR(v) = CDR(x);
   printf("redefined ");
   return v;
  }
  while (T(d) == CONS && !equ(v,car(CAR(d)))) d = CDR(d);
  if (T(d) == CONS) {
   CDR(CAR(d)) = x;
   printf("redefined ");
  }
  else env = pair(v,x,env);
 }
 return v;
}

/* section 11: additional Lisp primitives (optimized with evarg per section 16.4) */
L f_assoc(L t,L *e) { I a = 0; L v = evarg(&t,e,&a); return assoc(v,evarg(&t,e,&a)); }
L f_env(L _,L *e) { return *e; }
L f_let(L t,L *e) {
 L d = *e;
 for (; let(t); t = CDR(t))
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),eval(opt(CAR(t)),d),*e);
  else err(2,CAR(t));                           /* bound variable must be an atom */
 return car(t);
}
L f_letreca(L t,L *e) {
 for (; let(t); t = CDR(t)) {
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),nil,*e);
  else err(2,CAR(t));                           /* bound variable must be an atom */
  CDR(CAR(*e)) = eval(opt(CAR(t)),*e);
 }
 return car(t);
}
L f_letrec(L t,L *e) {
 L s,d,*p = &d;
 for (rc(p,*e),s = t; let(s); s = CDR(s))
  if (T(CAR(s)) == CONS && T(CAR(CAR(s))) == ATOM) p = &CDR(*p = pair(CAR(CAR(s)),nil,*e));
  else err(2,CAR(s));                           /* bound variable must be an atom */
 for (*e = d; let(t); t = CDR(t),d = CDR(d)) CDR(CAR(d)) = eval(opt(CAR(t)),*e);
 return rr(1,car(t));
}
L f_setq(L t,L *e) {
 L d = *e,v = car(t),x = eval(opt(t),d);
 while (T(d) == CONS && !equ(v,car(CAR(d)))) d = CDR(d);
 if (T(d) != CONS) err(2,v);
 return CDR(CAR(d)) = x;
}
L f_setcar(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = evarg(&t,e,&a); CAR(p) = x;
 return rr(1,x);
}
L f_setcdr(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = evarg(&t,e,&a); CDR(p) = x;
 return rr(1,x);
}
L f_macro(L t,L *_) { return macro(car(t),opt(t)); }
L f_print(L t,L *e) { I a = 0; L x; while (isarg(&t,e,&a,&x)) print(out,x); return nil; }
L f_println(L t,L *e) { f_print(t,e); fputc('\n',out); return nil; }

/* ++ new: atomize (stringify) x (to stringify the value of a variable v use (progn v) as argument) */
L f_atomize(L t,L *e) {
 I k; L s,*p = &s;
 for (rc(p,nil); T(t) == CONS; t = CDR(t))
  p = &CDR(*p = cons(T(CAR(t)) == ATOM || T(CAR(t)) == HOLD ? CAR(t) : eval(CAR(t),*e),nil));
 *p = t;                                        /* tail of s is t */
 k = atomize(s,NULL);                           /* the atom string length k, to hold atomized list of arguments */
 if (hp+k+17 > lp<<3) err(4,nil);               /* ERR 4 if the heap space is not large enough */
 atomize(s,A+hp);                               /* store the atomized arguments on the heap */
 return rr(1,atom(A+hp));                       /* this requires memmove() instead of strcpy() in atom() */
}

/* ++ updated: read from file with optional pathname argument converted using atomize */
L f_read(L t,L *e) {
 I i; L x; char c = see;
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 if (T(t) != NIL) {
  x = f_atomize(t,e);
  if (ld >= sizeof(in)/sizeof(*in) || !(in[ld++] = fopen(A+ord(x),"r"))) err(5,x);
 }
 see = 0;
 if ((i = setjmp(jb)) == 0) x = Read();
 memcpy(jb,savedjb,sizeof(jb));
 see = c;
 if (T(t) != NIL) fclose(in[--ld]);
 if (i) longjmp(jb,i);
 return x;
}

/* section 12: adding readline with history ++ updated: support multiple loads and nested loads */
L f_load(L t,L *e) {
 I j,k = ld; L s,v = nil;
 rc(&s,nil);
 while (T(t) == CONS) {
  s = CDR(t); CDR(t) = nil;                     /* temporarily set cdr(t) to nil */
  v = f_atomize(t,e);                           /* atomize one argument */
  t = CDR(t) = s;                               /* restore cdr(t) and visit next argument */
  if (ld >= sizeof(in)/sizeof(*in) || !(in[ld++] = fopen(A+ord(v),"r"))) err(5,v);
 }
 for (j = ld-1; j > k; --j,++k) { FILE *f = in[j]; in[j] = in[k]; in[k] = f; }  /* reverse the in[] additions */
 return rr(1,v);
}

/* section 13: execution tracing */
L f_trace(L t,L *_) { tr = not(t) ? !tr : (I)num(car(t)); return num(tr); }

/* section 14: error handling and exceptions */
L f_catch(L t,L *e) {
 I i; L x,**saved[2] = {sp,xp};                 /* save old stack pointers */
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 xp = sp;                                       /* set exception stack pointer xp = sp */
 if ((i = setjmp(jb)) == 0) x = eval(car(t),*e);
 memcpy(jb,savedjb,sizeof(jb));
 rr(sp-xp,nil);                                 /* deregister "lost" variables */
 sp = saved[0]; xp = saved[1];                  /* restore stack pointers */
 return i == 0 ? x : i == 4 || i == 6 ? err(i,nil) : cons(atom("ERR"),i);
}
L f_throw(L t,L *_) { return err(num(car(t)),nil); }

/* section 16.5: tail-call optimization */
L f_progn(L t,L *e) {
 for (; let(t); t = CDR(t)) eval(CAR(t),*e);
 return car(t);
}
L f_while(L t,L *e) {
 L s,x = nil,y;
 rc(&y,nil);
 while (!not(eval(car(t),*e)))
  for (s = cdr(t); T(s) == CONS; s = CDR(s),y = x) x = eval(CAR(s),*e);
 return rr(1,x);
}
L f_until(L t,L *e) {
 L s,x = nil;
 do for (s = t; T(s) == CONS; s = CDR(s)) x = eval(CAR(s),*e);
 while (not(x));
 return x;
}

/* ++ new: write the output of print/ln of a sequence of expressions to a file, append if the filename starts with a '+' */
L f_writeto(L t,L *e) {
 L x = cons(car(t),nil),y = nil,v = f_atomize(x,e); I i,k = *(A+ord(v)) == '+';
 FILE *savedout = out;                          /* save old out */
 jmp_buf savedjb;                               /* save old jmp buf */
 memcpy(savedjb,jb,sizeof(jb));
 if (!(out = fopen(A+ord(v)+k,k ? "a" : "w"))) err(5,v);        /* open file for writing or appending as new out */
 if ((i = setjmp(jb)) == 0) y = eval(f_progn(cdr(t),e),*e);     /* catch error in eval of progn of the rest of args */
 fclose(out);                                   /* close out */
 out = savedout;                                /* restore old out */
 memcpy(jb,savedjb,sizeof(jb));                 /* restore old jmp buf */
 if (i) longjmp(jb,i);                          /* re-throw error after garbage collecting y */
 return y;
}

/* ++ new: return the type of an expression, 0 = number, 1 = atom, 2 = primitive, 3 = pair, 4 = closure, 5 = macro, 6 = nil */
L f_type(L t,L *e) { I a = 0,k = T(cede(evarg(&t,e,&a))); return k >= ATOM && k <= NIL ? k-ATOM+1 : 0; }

/* ++ new: (number? x) returns #t if x is a number */
L f_numbert(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return x == x ? tru : nil; }

/* ++ new: (err? x) returns #t if x is ERR (NaN) */
L f_errt(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return equ(x,box(ATOM,0)) ? tru : nil; }

/* ++ new: (null? x) returns #t if x is nil () */
L f_nullt(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return T(x) == NIL ? tru : nil; }

/* ++ new: (symbol? v) returns #t if v is a symbol (an atom except ERR) */
L f_symbolt(L t,L *e) { I a = 0; L v = evarg(&t,e,&a); return T(v) == ATOM && ord(v) > 0 ? tru : nil; }

/* ++ new: (atom? x) returns #t if x is atomic (nil () or an atom except ERR) */
L f_atomt(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return T(x) == NIL ? tru : T(x) == ATOM && ord(x) > 0 ? tru : nil; }

/* ++ new: (list? t) returns #t if t is a proper list */
L f_listt(L t,L *e) { I a = 0; t = evarg(&t,e,&a); while (T(t) == CONS) t = CDR(t); return T(t) == NIL ? tru : nil; }

/* ++ new: (func? f) returns #t if f is a function (primitive or closure) */
L f_funct(L t,L *e) { I a = 0; L f = evarg(&t,e,&a); return T(f) == PRIM || T(f) == CLOS ? tru : nil; }

/* ++ new: (list ...) returns a list of its arguments (e.g. used in backquoting) */
L f_list(L t,L *e) { return evlis(t,*e); }

/* ++ new: (append ...) returns the concatenation of its list arguments as a new list (e.g. used in backquoting) */
L f_append(L t,L *e) {
 I a = 0; L x = nil,y,s,*p = &s;
 for (rc(&y,nil),rc(p,nil); isarg(&t,e,&a,&x) && !not(t); )
  for (y = x; !not(x); x = cdr(x)) p = &CDR(*p = cons(car(x),nil));
 *p = x;
 return rr(2,s);
}

/* ++ new: (length t) returns the length of list t */
L f_length(L t,L *e) {
 I a = 0,k = 0; L s = evarg(&t,e,&a);
 for (t = s; T(s) == CONS; s = CDR(s)) ++k;
 return k;
}

/* ++ new: (nthcdr n t) returns n'th rest of the list t */
L f_nthcdr(L t,L *e) {
 I a = 0,i = (I)num(evarg(&t,e,&a)); L s = evarg(&t,e,&a);
 for (t = s; i > 0; --i) t = cdr(t);
 return t;
}

/* ++ new: (nth n t) returns n'th item in list t */
L f_nth(L t,L *e) { return car(f_nthcdr(t,e)); }

/* ++ new: (last t [n]) returns last singleton list element of list t, optionally return list of n last list elements */
L f_last(L t,L *e) {
 I a = 0; L s,x,y; int n;
 rc(&x,evarg(&t,e,&a));
 n = isarg(&t,e,&a,&y) ? (int)num(y) : 1;
 for (t = s = x; T(t) == CONS; t = CDR(t)) if (n < 1) s = CDR(s); else --n;
 return rr(1,s);
}

/* ++ new: (reverse t) returns reversed copy of list t */
L f_reverse(L t,L *e) {
 I a = 0; L x,s = nil;
 for (rc(&x,evarg(&t,e,&a)),t = x; T(t) == CONS; t = CDR(t)) s = cons(CAR(t),s);
 return rr(1,s);
}

/* ++ new: (seq n m) returns list with the sequence (n n+1 n+2 ... m-1) */
L f_seq(L t,L *e) {
 I a = 0; int n = (int)num(evarg(&t,e,&a)),m = (int)num(evarg(&t,e,&a)); L s,*p = &s;
 for (rc(p,nil); n < m; ++n) p = &CDR(*p = cons(n,nil));
 return rr(1,s);
}

/* ++ new: (range n m k) returns list with the sequence (n n+k n+2k ... m-1) where optional k=1 by default */
L f_range(L t,L *e) {
 I a = 0; int n = (int)num(evarg(&t,e,&a)),m = (int)num(evarg(&t,e,&a)),k = 1; L x,s,*p = &s;
 if (isarg(&t,e,&a,&x)) k = (int)num(x);
 for (rc(p,nil); k*m > k*n; n += k) p = &CDR(*p = cons(n,nil));
 return rr(1,s);
}

/* ++ new: (equal? x y) deep check for equality, recurses up to lg(n) for n cons pairs, does not permit cyclic data */
I equal(L x,L y) {
 while (!equ(x,y) && T(x) == T(y) && (T(x) == CONS || T(x) == CLOS || T(x) == MACR)) {
  L u = CAR(x),v = CAR(y); x = CDR(x); y = CDR(y);
  if (T(u) != CONS && T(u) != CLOS && T(u) != MACR) {
   if (!equ(u,v)) return 0;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) {
   if (!equ(x,y)) return 0;
   x = u; y = v;
  }
  else if (!equal(u,v)) return 0;
 }
 return equ(x,y);
}
L f_equal(L t,L *e) {
 I a = 0; L x,z;
 rc(&x,evarg(&t,e,&a));
 z = equal(x,evarg(&t,e,&a)) ? tru : nil;
 return rr(1,z);
}

/* ++ new: (member x t) returns rest of list t from the first list element that is equal to x */
L f_member(L t,L *e) {
 I a = 0; L x;
 rc(&x,evarg(&t,e,&a));
 t = evarg(&t,e,&a);
 while (T(t) == CONS && !equal(x,CAR(t))) t = CDR(t);
 return rr(1,t);
}

/* ++ new: (copy-list t) returns a copy of list t */
L f_copylist(L t,L *e) {
 I a = 0; L x,s,*p = &s;
 for (t = rc(&x,evarg(&t,e,&a)); T(t) == CONS; t = CDR(t)) p = &CDR(*p = cons(CAR(t),nil));
 *p = t;
 return rr(1,s);
}

/* ++ new: (make-list n [x]) returns list of n copies of optional x, x is () by default */
L f_makelist(L t,L *e) {
 I a = 0; int n = (int)num(evarg(&t,e,&a)); L s = nil,x = nil;
 isarg(&t,e,&a,&x);
 while (n-- > 0) s = cons(x,s);
 return s;
}

/* ++ new: (> x y [z ...]) returns #t if x > y and y > z ... etc when given, otherwise returns () */
L f_gt(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a),y;
 while (isarg(&t,e,&a,&y)) if (lt(y,x)) x = y; else return nil;
 return tru;
}

/* ++ new: (<= x y [z ...]) returns #t if x <= y and y <= z ... etc when given, otherwise returns () */
L f_le(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a),y;
 while (isarg(&t,e,&a,&y)) if (!lt(y,x)) x = y; else return nil;
 return tru;
}

/* ++ new: (>= x y [z ...]) returns #t if x >= y and y >= z ... etc when given, otherwise returns () */
L f_ge(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a),y;
 while (isarg(&t,e,&a,&y)) if (!lt(x,y)) x = y; else return nil;
 return tru;
}

/* ++ new: (= x y) returns #t if number x equals number y, otherwise returns () */
L f_is(L t,L *e) { I a = 0; L x = num(evarg(&t,e,&a)); return x == num(evarg(&t,e,&a)) ? tru : nil; }

/* ++ new: (% x y ...) modulo of dividing x by y, then by ... */
L f_mod(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n %= (int64_t)num(x); return n; }

/* ++ new: (^ x y ...) raise x to the power y then by ... */
L f_exp(L t,L *e) { I a = 0; L x,n = evarg(&t,e,&a); while (isarg(&t,e,&a,&x)) n = pow(n,x); return num(n); }

/* ++ new: (<< x y ...) shift signed integer x left by y and by ... */
L f_lshift(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n <<= (int64_t)num(x); return n; }

/* ++ new: (>> x y ...) shift signed integer x right by y and by ... */
L f_rshift(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n >>= (int64_t)num(x); return n; }

/* ++ new: (& x y ...) bitwise and signed integers */
L f_bitand(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n &= (int64_t)num(x); return n; }

/* ++ new: (| x y ...) bitwise or signed integers */
L f_bitor(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n |= (int64_t)num(x); return n; }

/* ++ new: (~ x y ...) bitwise xor signed integers */
L f_bitxor(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n ^= (int64_t)num(x); return n; }

/* ++ new: (abs x) */
L f_abs(L t,L *e) { I a = 0; return num(fabs(evarg(&t,e,&a))); }

/* ++ new: (sgn x) */
L f_sgn(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return x > 0 ? 1 : x < 0 ? -1 : 0; }

/* ++ new: (neg x) */
L f_neg(L t,L *e) { I a = 0; return num(-evarg(&t,e,&a)); }

/* ++ new: (sqrt x) */
L f_sqrt(L t,L *e) { I a = 0; return num(sqrt(evarg(&t,e,&a))); }

/* ++ new: (sin x) */
L f_sin(L t,L *e) { I a = 0; return num(sin(evarg(&t,e,&a))); }

/* ++ new: (cos x) */
L f_cos(L t,L *e) { I a = 0; return num(cos(evarg(&t,e,&a))); }

/* ++ new: (tan x) */
L f_tan(L t,L *e) { I a = 0; return num(tan(evarg(&t,e,&a))); }

/* ++ new: (asin x) */
L f_asin(L t,L *e) { I a = 0; return num(asin(evarg(&t,e,&a))); }

/* ++ new: (acos x) */
L f_acos(L t,L *e) { I a = 0; return num(acos(evarg(&t,e,&a))); }

/* ++ new: (atan x) */
L f_atan(L t,L *e) { I a = 0; return num(atan(evarg(&t,e,&a))); }

/* ++ new: (atan2 x y) */
L f_atan2(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return num(atan2(x,evarg(&t,e,&a))); }

/* ++ new: (round x) */
L f_round(L t,L *e) { I a = 0; return num(round(evarg(&t,e,&a))); }

/* ++ new: (floor x) */
L f_floor(L t,L *e) { I a = 0; return num(floor(evarg(&t,e,&a))); }

/* ++ new: (ceiling x) */
L f_ceiling(L t,L *e) { I a = 0; return num(ceil(evarg(&t,e,&a))); }

/* ++ new: (char k [n]) return a string of n (default n=1) characters with code -128 <= k <= 255 */
L f_char(L t,L *e) {
 I a = 0,k = (int)num(evarg(&t,e,&a)),n = 1; L y;
 if (isarg(&t,e,&a,&y)) { n = num(y); if (n >= sizeof(buf)) n = sizeof(buf)-1; }
 buf[n] = '\0';
 while (n--) buf[n] = k;
 return atom(buf);
}

/* ++ new: (code <atom> [n]) return the code 0 to 255 of a single character in an atom at the front or at an optional given index n, returns 0 when beyond the end of the atom */
L f_code(L t,L *e) {
 I i,k,a = 0; L x,v = evarg(&t,e,&a);
 k = T(v) == ATOM ? strlen(A+ord(v)) : 0;
 i = isarg(&t,e,&a,&x) ? (I)num(x) : 0;
 return i < k ? *(A+ord(v)+i)&0xff : 0;
}

/* ++ new: (cpos <atom> <atom> [n]) return character position of the first <atom> in the second <atom> or nil (), look after position n */
L f_cpos(L t,L *e) {
 I i,a = 0; L x,v = evarg(&t,e,&a),w = evarg(&t,e,&a);
 i = isarg(&t,e,&a,&x) ? (I)num(x) : 0;
 if (T(v) == ATOM && T(w) == ATOM && i < strlen(A+ord(w))) {
  char *s = strstr(A+ord(w)+i,A+ord(v));
  if (s != NULL) return s-(A+ord(w));
 }
 return nil;
}

/* ++ new: (clen <atom>) return character length of <atom> */
L f_clen(L t,L *e) {
 I a = 0; L v = evarg(&t,e,&a);
 return T(v) == ATOM ? strlen(A+ord(v)) : 0;
}

#ifdef TIME
#include <sys/time.h>
/* ++ new: (time <expr> [n]) display running time of <expr> evaluated n (default n=1) times */
L f_time(L t,L *e) {
 L x = nil; I i,k = let(t) ? (I)num(car(CDR(t))) : 1;
 struct timeval tv0, tv1;
 float ms;
 gettimeofday(&tv0, NULL);
 for (i = 0; i < k; ++i) x = eval(car(t),*e);
 gettimeofday(&tv1, NULL);
 ms = tv1.tv_usec;
 ms -= tv0.tv_usec;
 ms = 1000.0 * (tv1.tv_sec - tv0.tv_sec) + ms/1000.0;
 if (ms < 0.0) ms += 60000.0;
 printf("\nelapsed time is %g ms\n",ms/k);
 return x;
}
#endif

L f_quit(L t,L *e) { I a = 0; L x; exit(isarg(&t,e,&a,&x) ? (int)num(x) : 0); }

struct { const char *s; L (*f)(L,L*); short t; } prim[] = {
 {"eval",     f_eval,    1},
 {"quote",    f_quote,   0},
 {"cons",     f_cons,    0},
 {"car",      f_car,     0},
 {"cdr",      f_cdr,     0},
 {"+",        f_add,     0},
 {"-",        f_sub,     0},
 {"*",        f_mul,     0},
 {"/",        f_div,     0},
 {"int",      f_int,     0},
 {"<",        f_lt,      0},
 {"eq?",      f_eq,      0},
 {"pair?",    f_pair,    0},
 {"or",       f_or,      0},
 {"and",      f_and,     0},
 {"not",      f_not,     0},
 {"cond",     f_cond,    1},
 {"if",       f_if,      1},
 {"let*",     f_leta,    1},
 {"lambda",   f_lambda,  0},
 {"define",   f_define,  0},
 {"assoc",    f_assoc,   0},
 {"env",      f_env,     0},
 {"let",      f_let,     1},
 {"letrec*",  f_letreca, 1},
 {"letrec",   f_letrec,  1},
 {"setq",     f_setq,    0},
 {"set-car!", f_setcar,  0},
 {"set-cdr!", f_setcdr,  0},
 {"macro",    f_macro,   0},
 {"read",     f_read,    0},
 {"print",    f_print,   0},
 {"println",  f_println, 0},
 {"load",     f_load,    0},
 {"catch",    f_catch,   0},
 {"throw",    f_throw,   0},
 {"trace",    f_trace,   0},
 {"progn",    f_progn,   1},
 {"while",    f_while,   0},
 {"until",    f_until,   0},
 {"list",     f_list,    0},
 {"append",   f_append,  0},
 {"atomize",  f_atomize, 0},
 {"write-to", f_writeto, 0},
 {"type",     f_type,    0},
 {"number?",  f_numbert, 0},
 {"err?",     f_errt,    0},
 {"null?",    f_nullt,   0},
 {"symbol?",  f_symbolt, 0},
 {"atom?",    f_atomt,   0},
 {"list?",    f_listt,   0},
 {"func?",    f_funct,   0},
 {"length",   f_length,  0},
 {"nthcdr",   f_nthcdr,  0},
 {"nth",      f_nth,     0},
 {"last",     f_last,    0},
 {"reverse",  f_reverse, 0},
 {"seq",      f_seq,     0},
 {"range",    f_range,   0},
 {"equal?",   f_equal,   0},
 {"member",   f_member,  0},
 {"copy-list",f_copylist,0},
 {"make-list",f_makelist,0},
 {">",        f_gt,      0},
 {"<=",       f_le,      0},
 {">=",       f_ge,      0},
 {"=",        f_is,      0},
 {"%",        f_mod,     0},
 {"^",        f_exp,     0},
 {"<<",       f_lshift,  0},
 {">>",       f_rshift,  0},
 {"&",        f_bitand,  0},
 {"|",        f_bitor,   0},
 {"~",        f_bitxor,  0},
 {"abs",      f_abs,     0},
 {"sgn",      f_sgn,     0},
 {"neg",      f_neg,     0},
 {"sqrt",     f_sqrt,    0},
 {"sin",      f_sin,     0},
 {"cos",      f_cos,     0},
 {"tan",      f_tan,     0},
 {"asin",     f_asin,    0},
 {"acos",     f_acos,    0},
 {"atan",     f_atan,    0},
 {"atan2",    f_atan2,   0},
 {"round",    f_round,   0},
 {"floor",    f_floor,   0},
 {"ceiling",  f_ceiling, 0},
 {"char",     f_char,    0},
 {"code",     f_code,    0},
 {"cpos",     f_cpos,    0},
 {"clen",     f_clen,    0},
#ifdef TIME
 {"time",     f_time,    0},
#endif
 {"quit",     f_quit,    0},
 {0}};

/* section 13: tracing (trace 1) with colorful output, to wait on ENTER (trace 2), with memory dump (trace 3) */
void trace(L y,L x,L e) {
 if (tr > 2 && !equ(e,env)) {
  printf("\n\e[35mENV: \e[33m");
  for (; !equ(e,env); e = cdr(e)) {
   print(stdout,car(car(e))); printf("\e[36m = \e[33m"); print(stdout,cdr(car(e))); printf("   ");
  }
  printf("\e[m");
 }
 printf("\n\e[32m%u \e[33m",lp); print(stdout,y); printf("\e[36m => \e[33m"); print(stdout,x); printf("\e[m\t");
 if (tr > 1) while (getchar() >= ' ') continue;
}

/* section 16.2/3/4: tail-call optimization (section 17.2: hygienic macros - remove MACR branch) */
L eval(L x,L e) {
 I a; L d,f,g,v,y;
 /* if x is an atom, then return its value; if x is not an application list (it is constant), then return x */
 if (T(x) == ATOM) return assoc(x,e);
 if (T(x) != CONS) return x;
 /* pre-check for stack overflow, expect 3 + 1 (for evlis) rc() calls to register variables */
 if (sp >= stk+S-4) return err(4,nil);
 rc(&d,nil); rc(&e,e); rc(&g,nil);
 while (1) {
  /* copy x to y to output y => x when tracing is enabled */
  y = x;
  /* if x is an atom, then return its value; if x is not an application list (it is constant), then return x */
  if (T(x) == ATOM) { x = assoc(x,e); break; }
  if (T(x) != CONS) break;
  /* evaluate f in the application (f . x) and get the list of arguments x */
  g = nil; f = CAR(x); x = CDR(x);
  if (T(f) == ATOM) f = assoc(f,e);
  else if (T(f) == CONS) f = g = eval(f,e);
  if (T(f) == PRIM) {
   /* apply Lisp primitive to argument list x, return value in x */
   x = prim[ord(f)].f(x,&e);
   /* if tail-call then continue evaluating x, otherwise return x */
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) != CLOS) return err(3,f);
  /* get the list of variables v of closure f and its local environment d (use global env when nil) */
  d = CDR(f); f = CAR(f);
  if (T(d) == NIL) d = env;
  /* bind closure f variables v to the evaluated argument values */
  for (a = 0,v = CAR(f); T(v) == CONS; v = CDR(v)) d = pair(CAR(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? x : evlis(x,e),d);
  /* next, evaluate body x of closure f in environment e = d */
  x = CDR(f); e = d;
  if (tr) trace(y,x,e);
 }
 if (tr && !equ(x,y)) trace(y,x,e);
 /* deregister variables */
 return rr(3,x);
}

/* section 12: adding readline with history */
void look() {
 while (ld) {
  int c;
  if (!in[--ld]) err(5,nil);
  see = c = getc(in[ld++]);
  if (c != EOF) return;
  fclose(in[--ld]);
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
L endl(L t) { return scan() == ')' ? t : err(7,t); }            /* ERR 7 when closing ) is missing */
L list() {
 L t,*p = &t;
 for (rc(p,nil); ; p = &CDR(*p = cons(parse(),nil))) {
  if (scan() == ')') return rr(1,t);
  if (*buf == '.' && !buf[1]) { *p = Read(); return rr(1,endl(t)); }
 }
}
L tick() {
 L t,*p;
 if (*buf == ',') return Read();
 if (*buf == '\'') { scan(); rc(&t,cons(tick(),nil)); t = cons(atom("list"),cons(quote(atom("quote")),t)); return rr(1,t); }
 if (*buf == '"') return parse();
 if (*buf == ')') return err(7,atom(buf));
 if (*buf != '(') return quote(parse());
 for (p = &CDR(rc(&t,cons(atom("list"),nil))); ; p = &CDR(*p = cons(tick(),nil))) {
  if (scan() == ')') return rr(1,t);
  if (*buf == '.' && !buf[1]) { scan(); t = endl(cons(atom("append"),cons(t,cons(tick(),nil)))); return rr(1,t); }
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
 while (T(e) == CONS && !equ(v,car(CAR(e)))) e = CDR(e);
 if (T(e) != CONS) return 0;
 *x = cdr(CAR(e));
 return 1;
}
/* section 17.2: hygienic macros */
/* return a copy of expression of x that holds all variables (atoms) by placing each ATOM in a HOLD */
L hold(L x) {
 if (T(x) == ATOM) return box(HOLD,ord(x));
 if (T(x) == CONS) {
  L t,*p = &t;
  for (rc(p,nil); T(x) == CONS; x = CDR(x)) p = &CDR(*p = cons(hold(CAR(x)),nil));
  *p = hold(x);
  return rr(1,t);
 }
 return x;
}
/* check if expression x has variable (atom) v placed on HOLD */
I holds(L v,L x) {
 if (T(x) == HOLD) return ord(v) == ord(x);
 if (T(x) == CONS) return holds(v,CAR(x)) || holds(v,CDR(x));
 return 0;
}
/* generate a new symbol for atom v by prepending an underscore _ to the symbol v */
L gensym(L v) { char sym[sizeof(buf)]; snprintf(sym,sizeof(sym),"_%s",A+ord(v)); return atom(sym); }
/* if necessary rename variable(s) v to prevent "variable shadowing" in macro-expanded expression x */
L hygienic(L v,L x) {
 if (T(v) == CONS) {
  L w,*p = &w;
  for (rc(p,nil); T(v) == CONS; v = CDR(v)) p = &CDR(*p = cons(hygienic(CAR(v),x),nil));
  if (T(v) == ATOM || T(v) == HOLD) *p = hygienic(v,x);
  return rr(1,w);
 }
 if (T(v) == HOLD) return box(ATOM,ord(v));
 if (T(v) == ATOM) while (holds(v,x)) v = gensym(v);
 return v;
}
/* return HOLD v as an ATOM v, otherwise just return v */
L cede(L v) { return T(v) == HOLD ? box(ATOM,ord(v)) : v; }
/* release all variables held in x; this operation destructively replaces each HOLD by ATOM throughout x */
L release(L x) {
 if (T(x) == CONS) {
  L *p = &x;
  for (; T(*p) == CONS; p = &CDR(*p)) CAR(*p) = release(CAR(*p));
  *p = release(*p);
 }
 return cede(x);
}
/* section 17.1-2: early binding and efficient macro expansion with hygienic macros */
L expand(L x,L e,L b) {
 L c,d,v,w,y,z;
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
 rc(&c,nil); rc(&d,nil);
 if (T(x) == CLOS) {
  /* expand the body of closure x */
  v = w = car(CAR(x));                  /* the variables v of the closure */
  d = e = CDR(x);                       /* the lexical scope of bindings d of the closure */
  y = cdr(CAR(x));                      /* the body y of the closure */
  if (T(d) == NIL) d = env;             /* closure has global scope */
  /* closure variables v hide macro variables in b and hide global primitives and macros in d */
  for (c = b; T(v) == CONS; v = CDR(v)) d = pair(CAR(v),nil,d),c = pair(CAR(v),CAR(v),c);
  if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,v,c);
  /* expand the body y of the closure and create a new closure with variables w and lexical scope e */
  z = closure(w,expand(y,d,c),e);
  return rr(2,z);
 }
 if (T(x) == CONS) {
  /* expand the application x = (f ...) in which we first expand f */
  L t,*p,f = expand(CAR(x),e,b);
  /* then we construct a new application list t = (f ...) by populating *p = ... with the list of expanded arguments */
  rc(&t,cons(f,nil)); p = &CDR(t);
  x = CDR(x);
  if (T(f) == MACR) {
   /* f in (f ...) is a macro to apply by expand/eval/expand its body */
   I i; jmp_buf savedjb;
   memcpy(savedjb,jb,sizeof(jb));
   /* bind the variables v of macro f to the given arguments x quoted (and hold all atoms in x) in environment c */
   for (c = nil,v = release(CAR(f)); T(v) == CONS && T(x) == CONS; v = CDR(v),x = CDR(x))
    c = pair(CAR(v),quote(hold(CAR(x))),c);
   if (T(v) == ATOM) c = pair(v,quote(hold(x)),c);
   else if (T(v) != NIL) err(8,nil);
   /* expand macro body CDR(f) using macro arguments bound in updated environment c */
   rc(&x,expand(CDR(f),e,c));
   /* eval macro body (may fail) then expand the result with macro arguments bound in environment b */
   if ((i = setjmp(jb)) == 0) rc(&y,eval(x,e));
   memcpy(jb,savedjb,sizeof(jb));
   if (i) {
    printf("\e[31;1mmacro expansion failed:\e[m "); print(stdout,x); printf("\n");
    longjmp(jb,i);
   }
   z = expand(y,e,b);
   return rr(5,z);
  }
  if (T(f) == PRIM) {
   /* f is a primitive in (f ...) */
   if (equ(f,p_quote)) {                /* <quote>: release variables, but do not expand */
    *p = release(x);
    return rr(2,t);
   }
   if (equ(f,p_macro)) {                /* <macro>: expand body */
    *p = cons(car(x),cons(expand(opt(x),e,b),nil));
    return rr(3,t);
   }
   if (equ(f,p_lambda)) {
    /* <lambda> arguments v hide macro variables in b and hide global primitives and macros in e */
    v = car(x); w = hygienic(v,cdr(x));
    *p = cons(w,nil);
    for (d = e,c = b; T(v) == CONS; v = CDR(v),w = CDR(w))
     if (T(CAR(v)) == ATOM) d = pair(CAR(v),nil,d),c = pair(CAR(v),CAR(w),c);
    if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    CDR(*p) = cons(expand(opt(x),d,c),nil);             /* expand <lambda> body */
    return rr(3,t);
   }
   if (equ(f,p_cond)) {
    /* <cond> arguments are pairs of expressions (each cons pair is not a function application) */
    rc(&y,nil); rc(&z,nil);
    for (; T(x) == CONS; x = CDR(x)) {
     y = expand(car(CAR(x)),e,b); z = expand(opt(CAR(x)),e,b);
     p = &CDR(*p = cons(cons(y,cons(z,nil)),nil));
    }
    return rr(5,t);
   }
   if (equ(f,p_leta)) {
    /* <let*> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = CDR(x)) {
     v = car(CAR(x)); w = hygienic(v,CDR(x));
     p = &CDR(*p = cons(cons(w,cons(expand(opt(CAR(x)),d,c),nil)),nil));
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <let*> body */
    return rr(3,t);
   }
   if (equ(f,p_let)) {
    /* <let> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = CDR(x)) {
     v = car(CAR(x)); w = hygienic(v,CDR(x));
     p = &CDR(*p = cons(cons(w,cons(expand(opt(CAR(x)),e,c),nil)),nil));
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <let> body */
    return rr(3,t);
   }
   if (equ(f,p_letreca)) {
    /* <letrec*> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b; let(x); x = CDR(x)) {
     v = car(CAR(x)); w = hygienic(v,x);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
     p = &CDR(*p = cons(cons(w,cons(expand(opt(CAR(x)),d,c),nil)),nil));
    }
    *p = cons(expand(car(x),d,c),nil);                  /* expand <letrec*> body */
    return rr(3,t);
   }
   if (equ(f,p_letrec)) {
    /* <letrec> local variables hide macro variables in b and hide global primitives and macros in e */
    for (d = e,c = b,y = x; let(y); y = CDR(y)) {
     v = car(CAR(y)); w = hygienic(v,x);
     if (T(v) == ATOM) d = pair(v,nil,d),c = pair(v,w,c);
    }
    for (y = x; let(y); y = CDR(y)) {
     v = car(CAR(y)); w = hygienic(v,x);
     p = &CDR(*p = cons(cons(w,cons(expand(opt(CAR(y)),d,c),nil)),nil));
    }
    *p = cons(expand(car(y),d,c),nil);                  /* expand <letrec> body */
    return rr(3,t);
   }
   if (equ(f,p_define)) {
    /* <define> early bind self-recursive calls in functions to its closure of the function */
    v = expand(car(x),e,b);                             /* expand variable v of (<define> v x) */
    *p = cons(v,nil);                                   /* to return expanded (<define> v ...) */
    x = opt(x);                                         /* body x of (<define> v x) */
    if (T(v) == ATOM) {                                 /* if v is an atom then ... */
     f = closure(nil,nil,nil);                          /* v may reference itself, assume it's a function */
     d = pair(v,f,e);                                   /* update environment d of e to include (v . f) */
     rc(&y,expand(x,d,b));                              /* y is expanded body x of (<define> v x) */
     z = eval(y,e);                                     /* evaluate expanded y of body x */
     if (T(z) == CLOS) {                                /* if this is a closure then ... */
      CAR(f) = CAR(z); CDR(f) = CDR(z);                 /* replace closure f's variables, body, and env with z's */
      CDR(*p) = cons(f,nil);                            /* to return expanded (<define> v f) with closure f */
     }
     else CDR(*p) = cons(y,nil);                        /* to return expanded (<define> v y) */
     rr(1,nil);
    }
    else CDR(*p) = cons(expand(x,e,b),nil);             /* to return expanded (<define> v y) */
    return rr(3,t);
   }
  }
  /* expand argument expressions x in (f . x) and return a new application t = (f ...) by populating *p */
  for (; T(x) == CONS; x = CDR(x)) p = &CDR(*p = cons(expand(CAR(x),e,b),nil));
  *p = expand(x,e,b);
  return rr(3,t);
 }
 return rr(2,x);
}

/* section 8: printing Lisp expressions */
void printlist(FILE *f,L t) {
 fputc('(',f);
 while (1) {
  print(f,CAR(t));
  if (not(t = CDR(t))) break;
  if (T(t) != CONS) { fprintf(f," . "); print(f,t); break; }
  fputc(' ',f);
 }
 fputc(')',f);
}
/* ++ new: display closure or macro, with its name if in the global environment */
void printpair(FILE *f,const char c[2],L x) {
 L e = env;
 while (T(e) == CONS && !equ(x,CDR(CAR(e)))) e = CDR(e);
 if (T(e) == CONS && T(CAR(CAR(e))) == ATOM) fprintf(f,"%c%s%c",c[0],A+ord(CAR(CAR(e))),c[1]);
 else fprintf(f,"%c%u%c",c[0],ord(x),c[1]);
}
void print(FILE *f,L x) {
 if (T(x) == NIL) fprintf(f,"()");
 else if (T(x) == ATOM) fprintf(f,"%s",A+ord(x));
 else if (T(x) == PRIM) fprintf(f,"<%s>",prim[ord(x)].s);
 else if (T(x) == CONS) printlist(f,x);
 else if (T(x) == CLOS) printpair(f,"{}",x);
 else if (T(x) == MACR) printpair(f,"[]",x);
 else if (T(x) == HOLD) fprintf(f,"|%s|",A+ord(x));
 else fprintf(f,"%.10lg",x);
}

/* ++ new: atomize (stringify) x to buffer a when not NULL, must be large enough to hold the string, return string length */
I atomize(L x,char *a) {
 if (T(x) == CONS) {
  I i,k = 0;
  for (; T(x) == CONS; x = CDR(x)) {
   k += i = atomize(CAR(x),a);
   if (a) a += i;
  }
  if (T(x) != NIL) {
   k += i = atomize(x,a);
   if (a) a += i;
  }
  return k;
 }
 if (T(x) == ATOM || T(x) == HOLD) return strlen(a ? strcpy(a,A+ord(x)) : A+ord(x));
 if (x == x) snprintf(buf,sizeof(buf),"%.10lg",x); else strcpy(buf," ");
 return strlen(a ? strcpy(a,buf) : buf);
}

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; printf("tinylisp-extras-expand-ms");
 /* clear stack and memory */
 env = nil; gc();
 atom("ERR"); atom("#t"); env = pair(tru,tru,nil);
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
 p_define  = assoc(atom("define"),env);
 /* read input file */
 in[ld++] = fopen((argc > 1 ? argv[1] : "common.lisp"),"r");
 using_history();
 signal(SIGINT,stop);
 if ((i = setjmp(jb)) > 0) {
  while (ld) if (in[--ld]) fclose(in[ld]);
  printf("ERR %u",i);
  if (i == 7) see = 0;
 }
 out = stdout;
 while (1) {
  L x,y;
  gc();
  putchar('\n'); snprintf(ps,sizeof(ps),PS1,2*fn-hp/8);
  /* section 17.1: early binding and efficient macro expansion (REEPL = REPL with expand) */
  print(out,eval(rc(&y,expand(rc(&x,Read()),env,nil)),env));
 }
}
