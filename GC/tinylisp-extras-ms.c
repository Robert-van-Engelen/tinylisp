/* tinylisp-extras-ms.c optimized and article's extras and more, using mark-sweep GC by Robert A. van Engelen 2026 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
        i      any unsigned integer, e.g. a NaN-boxed ordinal value or index
        t      a NaN-boxed tag
        a      dot operator argument flag, used with evarg()
   L variables and function parameters are named as follows:
        x,y    any Lisp expression
        n      number
        t,s    list
        f,g    function, a lambda closure or Lisp primitive or macro
        h      macro body expression, used in eval()
        p      pair, a cons of two Lisp expressions
        e,d    environment, a list of pairs, e.g. created with (define v x)
        v      the name of a variable (an atom) or a list of variables */
#define I uint32_t
#define L double

/* address of the atom heap is at the bottom of the cell stack */
#define A (char*)cell

/* number of cells for the shared stack of cells and atom heap, increase N as desired */
#define N 8192

/* section 12: adding readline with history */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in[10],*out;
char buf[256],see = 0,*ptr = "",*line = NULL,ps[80];

/* prompt strings for readline (truncates to 80 chars max), use \001 to ignore codes up to \002 */
/* NOTE: MacOS Darwin uses libedit as a libreadline "compatible", but that does not display prompt colors! */
#define PS1 "\001\e[32;1m\002%u>\001\e[m\002"
#define PS2 "\001\e[32;1m\002? \001\e[m\002"

/* forward proto declarations */
L eval(L,L),Read(),parse(),err(I,L); void ms(L),print(FILE*,L),stop(int); I atomize(L,char*);

/* section 4: constructing Lisp expressions */
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
enum { ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd };
/* cell[N] array of Lisp expressions, shared by the stack and atom heap */
L cell[N];
/* bits[] bit array for marking used cell pairs in mark-sweep garbage collection */
I bits[((N/2)+31)/32];
/* check if the cell pair (cell[i],cell[i+1]) is used and to mark it as used */
I used(I i) { return bits[i/64]&(1<<i/2%32); }
void mark(I i) { bits[i/64] |= 1<<i/2%32; }
/* Lisp constant expressions () (nil), #t, and the global environment env */
L nil,tru,env;
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
 I i = 0;
 while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp) {
  if ((hp += strlen(s)+1)+16 > lp<<3) ms(nil);
  memmove(A+i,s,hp-i);
 }
 return box(ATOM,i);
}

/* ++ new: mark-sweep garbage collector registry stack size S, max depth of nested calls to eval() = S/4 */
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
#ifdef PR                                               /* safe non-recursive pointer reversal method */
void mk(L x) {
 I i,j = N,k;
 if (used(i = ord(x))) return;
 while (j < N || !(i&1)) {                              /* repeat until all reachable cell pairs are marked */
  while (1) {                                           /* go down the list, marking cdr first before car */
   mark(i); x = cell[i];                                /* x is the cdr cell (even i) */
   if ((T(x) != CONS && T(x) != CLOS && T(x) != MACR) || used(k = ord(x))) {
    x = cell[++i];                                      /* x is the car cell (odd i) */
    if ((T(x) != CONS && T(x) != CLOS && T(x) != MACR) || used(k = ord(x))) break;
   }
   cell[i] = box(T(cell[i]),j);                         /* reverse the cdr (even i) or the car (odd i) pointer */
   j = i; i = k;                                        /* last cell visited j, cell to visit i (i is even) */
  }
  while (j < N) {                                       /* go back up via reversed pointers until the root */
   k = i; i = j; j = ord(cell[i]);                      /* last cell visited k, cell to visit i, next cell to visit j */
   cell[i] = box(T(cell[i]),k&~1);                      /* un-reverse the cdr (even i) or car (odd i) pointer */
   if (!(i&1)) break;                                   /* if i'th cell is a cdr (even i), then break to go down again */
  }
 }
}
#else                                                   /* recursive method, may recurse too deep for large N (unsafe) */
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
#endif
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
  if (used(i) && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
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
   ERR 5: cannot open
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
L assoc(L v,L e) {
 for (; T(e) == CONS && T(CAR(e)) == CONS; e = CDR(e)) if (equ(v,CAR(CAR(e)))) return CDR(CAR(e));
 return err(2,v);
}
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
L f_lt(L t,L *e) {
 I a = 0; L x = evarg(&t,e,&a),y;
 while (isarg(&t,e,&a,&y)) {
  if (T(x) == ATOM && T(y) == ATOM ? strcmp(A+ord(x),A+ord(y)) >= 0 :
      x == x && y == y ? x >= y :
      T(x) >= T(y) || (T(x) == T(y) && ord(x) >= ord(y))) return nil;
  x = y;
 }
 return tru;
}
L f_eq(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return equ(x,evarg(&t,e,&a)) ? tru : nil; }
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
/* define a global symbol, garbage collect the old unreachable definitions when redefined */
L f_define(L t,L *e) {
 L d = env,v = car(t),x;
 if (T(v) != ATOM) return err(2,v);             /* bound variable must be an atom, to prevent GC issues when not an atom */
 x = eval(opt(t),*e);
 while (T(d) == CONS && !equ(v,car(CAR(d)))) d = CDR(d);
 if (T(d) != CONS) env = pair(v,x,env);
 else {
  L *p = &CDR(CAR(d));
  if (T(*p) != PRIM) { *p = x; printf("redefined "); }
  else printf("not redefined built-in ");
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
  p = &CDR(*p = cons(T(CAR(t)) == ATOM ? CAR(t) : eval(CAR(t),*e),nil));
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
L f_type(L t,L *e) { I a = 0,k = T(evarg(&t,e,&a)); return k >= ATOM && k <= NIL ? k-ATOM+1 : 0; }

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
 {"catch",   f_catch,  0},
 {"throw",   f_throw,  0},
 {"trace",   f_trace,  0},
 {"progn",   f_progn,  1},
 {"while",   f_while,  0},
 {"until",   f_until,  0},
 {"atomize", f_atomize,0},
 {"write-to",f_writeto,0},
 {"type",    f_type,   0},
 {"list",    f_list,   0},
 {"append",  f_append, 0},
 {"quit",    f_quit,   0},
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

/* section 16.2/3/4: tail-call optimization */
L eval(L x,L e) {
 I a; L d,f,v,y;
 /* if x is an atom, then return its value; if x is not an application list (it is constant), then return x */
 if (T(x) == ATOM) return assoc(x,e);
 if (T(x) != CONS) return x;
 /* pre-check for stack overflow, expect 4 + 1 (for evlis) rc() calls to register variables */
 if (sp >= stk+S-5) return err(4,nil);
 rc(&d,nil); rc(&e,e); rc(&f,nil); rc(&y,nil);
 while (1) {
  y = x;
  /* if x is an atom, then return its value; if x is not an application list (it is constant), then return x */
  if (T(x) == ATOM) { x = assoc(x,e); break; }
  if (T(x) != CONS) break;
  /* evaluate f in the application (f . x) and get the list of arguments x */
  f = CAR(x); x = CDR(x);
  f = T(f) == ATOM ? assoc(f,e) : T(f) == CONS ? eval(f,e) : f;
  if (T(f) == PRIM) {
   /* apply Lisp primitive to argument list x, return value in x */
   x = prim[ord(f)].f(x,&e);
   /* if tail-call then continue evaluating x, otherwise return x */
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) == MACR) {
   /* bind macro f variables v to the given arguments literally (i.e. without evaluating the arguments) */
   for (d = env,v = CAR(f); T(v) == CONS && T(x) == CONS; v = CDR(v),x = CDR(x)) d = pair(CAR(v),CAR(x),d);
   if (T(v) == ATOM) d = pair(v,x,d);
   else if (T(v) != NIL) err(8,nil);
   /* expand macro f, then continue evaluating the expanded x, discard d, looping with y = x saves this macro */
   x = eval(CDR(f),d); d = nil;
   continue;
  }
  if (T(f) != CLOS) return err(3,f);
  /* get the list of variables v of closure f and its local environment d (use global env when nil) */
  v = CAR(CAR(f)); d = CDR(f);
  if (T(d) == NIL) d = env;
  /* bind closure f variables v to the evaluated argument values */
  for (a = 0; T(v) == CONS; v = CDR(v)) d = pair(CAR(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? x : evlis(x,e),d);
  /* next, evaluate body x of closure f in environment e = d */
  x = CDR(CAR(f)); e = d;
  if (tr) trace(y,x,e);
 }
 if (tr && !equ(x,y)) trace(y,x,e);
 /* deregister variables */
 return rr(4,x);
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
void print(FILE *f,L x) {
 if (T(x) == NIL) fprintf(f,"()");
 else if (T(x) == ATOM) fprintf(f,"%s",A+ord(x));
 else if (T(x) == PRIM) fprintf(f,"<%s>",prim[ord(x)].s);
 else if (T(x) == CONS) printlist(f,x);
 else if (T(x) == CLOS) fprintf(f,"{%u}",ord(x));
 else if (T(x) == MACR) fprintf(f,"[%u]",ord(x));
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
 if (T(x) == ATOM) return strlen(a ? strcpy(a,A+ord(x)) : A+ord(x));
 if (x == x) snprintf(buf,sizeof(buf),"%.10lg",x); else strcpy(buf," ");
 return strlen(a ? strcpy(a,buf) : buf);
}

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; printf("tinylisp-extras-ms");
 /* clear stack and memory */
 env = 0; gc();
 nil = box(NIL,0); atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
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
  L x;
  gc();
  putchar('\n'); snprintf(ps,sizeof(ps),PS1,2*fn-hp/8);
  print(out,eval(rc(&x,Read()),env));
 }
}
