/* tinylisp-extras-gc.c optimized, article's extras and more, using ref count GC by Robert A. van Engelen 2025 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h> /* to return NAN from num() */

/* DEBUG: enable memory management activity logging (=1 enable, =2 include Lisp expression dumps) */
#if DEBUG == 0
# define LOG(x,...) (void)0
#elif DEBUG == 1
# define LOG(x,...) printf(__VA_ARGS__)
#else
# define LOG(x,...) (printf(__VA_ARGS__),printf("\e[33m"),print(stdout,x),printf("\e[m\t"))
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
        h      macro body expression, used in eval()
        p      pair, a cons of two Lisp expressions
        e,d    environment, a list of pairs, e.g. created with (define v x)
        v      the name of a variable (an atom) or a list of variables */
#define I uint32_t
#define L double

/* address of the atom heap is at the bottom of the cell pool */
#define A (char*)cell

/* number of cells for the shared pool and atom heap, increase N as desired */
#define N 8192

/* forward proto declarations */
L eval(L,L),Read(),parse(),err(I,L),gc(L); void collect(L),print(FILE*,L); I atomize(L,char*);

/* section 4: constructing Lisp expressions (using a cell pool managed with reference count garbage collection) */
/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   fp: free cell pairs list pointer, ref[fp/2] is the head of the linked list of free cell pairs
   lp: pointer to the lowest allocated and used cell pair in cell[]
   fn: number of free cell cons pairs in cell[] (not taking atoms stored in cell[] into account)
   tr: tracing off (0), on (1), wait on ENTER (2), dump and wait (3)
   ld: number of open loads from input files (nested load up to 10 levels deep)
   safety invariant: hp < lp<<3 */
I hp = 0,fp = N-2,lp = N-2,fn = N/2,tr = 0,ld = 0;
/* ref[] array with ref count of a used cell pair or ref to next free cell pair in the free list */
I ref[N/2];
/* atom, primitive, cons, closure and nil tags for NaN boxing */
enum { ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd };
/* cell[N] pool of allocatable Lisp expressions shared by the atom heap */
L cell[N];
/* Lisp constant expressions () (nil), #t, and the global environment env */
L nil,tru,env;
/* NaN-boxing specific functions:
   T(x):     returns the tag bits of a NaN-boxed double x
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   check number, return NAN = ERR = box(ATOM,0) when not a number to avoid ref-count GC on a list when n is a list
   equ(x,y): returns nonzero if x equals y */
I T(L x) { union { L x; uint64_t i; } u = {x}; return u.i>>48; }
L box(I t,I i) { union { uint64_t i; L x; } u = {(uint64_t)t<<48|i}; return u.x; }
I ord(L x) { union { L x; uint64_t i; } u = {x}; return u.i; }
L num(L n) { return n == n ? n : NAN; }
I equ(L x,L y) { union { L x; uint64_t i; } u = {x},v = {y}; return u.i == v.i; }
/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 return i == hp && ((hp += strlen(s)+1) > lp<<3 || !strcpy(A+i,s)) ? err(4,nil) : box(ATOM,i);
}

/* section 12: adding readline with history ++ new: support nested load, new err 5 can't open file */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in[10],*out;
char buf[256],see = ' ',*ptr = "",*line = NULL,ps[80];

/* prompt strings for readline (truncates to 80 chars max), use \001 to ignore codes up to \002 */
/* NOTE: MacOS Darwin uses libedit as a libreadline "compatible", but that does not display prompt colors! */
#define PS1 "\001\e[32;1m\002%u>\001\e[m\002"
#define PS2 "\001\e[32;1m\002? \001\e[m\002"

/* section 14: error handling and exceptions
   ERR 1: not a pair
   ERR 2: unbound symbol
   ERR 3: cannot apply
   ERR 4: out of memory
   ERR 5: cannot open
   ERR 6: program stopped */
#include <setjmp.h>
#include <signal.h>
/* max number of nested eval() calls between f_catch and f_throw */
#define K 1024
/* exception stack and pointers to track "lost" variables between f_catch() and err() to garbage collect */
L *xstk[5*K],**xb = NULL,**xp = NULL;
jmp_buf jb;
/* throw an error, if f_catch handler is used (xb < xp are not NULL) then garbage collect "lost" variables */
L err(I i,L x) {
 const char *msg[6] = {"not a pair","unbound","cannot apply","out of memory","cannot open","stopped"};
 if (xp ? tr : i >= 1 && i <= 6) {
  printf("\n\e[31;1mERR %u: ",i); print(stdout,x); printf(" %s\e[m\n",i >= 1 && i <= 6 ? msg[i-1] : "");
 }
 while (xp != xb) gc(**--xp);
 longjmp(jb,i);
}
/* register x with initial value y to collect with rg(x) or in a f_catch exception handler when an error occurred */
void rc(L *x,L y) { *x = y; if (xp) *xp = x,++xp; }     /* GCC incorrectly warns about *xp++ = x dangling pointer */
/* remove x from catch-throw registry and garbage collect x */
L rg(L x) { if (xp) --xp; return gc(x); }
/* remove n registrations without garbage collecting them */
void rr(I k) { if (xp) xp -= k; }

/* memory management with ref[] array using free and SCC marker bits */
const I FREE = ~((I)~0UL>>1),MARK = FREE,SCC = MARK>>1;
/* lowest pointer to allocated cells in memory */
I lomem(I i) { return lp = i < lp ? i : lp; }
/* allocate a new pair */
L alloc() { I i = fp; fp = ref[i/2] & ~FREE; ref[i/2] = 1; --fn; return hp > lomem(i)<<3 ? err(4,nil) : box(CONS,i); }

/* unsafe fast car and cdr, must be guarded to use: if (T(x) == CONS) { ... CAR(x) ... CDR(x) ... } */
#define CAR(p) cell[ord(p)+1]
#define CDR(p) cell[ord(p)]

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(L x,L y) { L p = alloc(); I i = ord(p); cell[i+1] = x; cell[i] = y; LOG(p,"\n\e[32mcons %u\e[m\t",i); return p; }
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
/* let(x) is nonzero if x has more than one list item, used by let* */
I let(L x) { return T(x) == CONS && T(CDR(x)) == CONS; }

/* duplicate expression x: if x is a pair then increment its ref count by one */
L dup(L x) {
 if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
  I i = ord(x);
  if (ref[i/2] & SCC) i = ref[i/2] & ~SCC;              /* if x is in an SCC then update SCC representative ref count */
  ++ref[i/2];                                           /* increment ref count */
  LOG(x,"\n\e[32m++#%u=%u\e[m\t",i,ref[i/2]);
 }
 return x;
}

/* garbage collect: if x is a pair then collect pair x by decrementing its ref count, deleting it if count drops to 0 */
L gc(L x) { if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) collect(x); return x; }
/* delete the pair cell[i] cell[i+1] to reuse by adding it to the free cell pair list */
void del(I i) { ref[i/2] = FREE|fp; fp = i; ++fn; }
/* from cell pair x onwards, delete entire SCC identified by representative k with SCC bit set, gc non-SCC branches */
void delscc(I k,L x) {
 I i; L y;
 while (!(ref[(i = ord(x))/2] & FREE)) {                /* repeat until all SCC cell pairs x are deleted */
  LOG(x,"\n\e[36mfree %u\e[m\t",i);
  del(i);                                               /* delete the SCC cell pair x to reuse */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
    if (ref[ord(y)/2] != k) collect(y);                 /* only cdr(x) is part of the SCC, car(x) is a pair */
    else if (ref[ord(x)/2] == k) delscc(k,y);           /* both car(x) and cdr(x) are part of the SCC k */
    else collect(x),x = y;                              /* only car(x) is part of the SCC, cdr(x) is a pair */
   }
   else x = y;                                          /* only car(x) is part of the SCC k */
  }
 }
}
/* collect pair x: decrement ref count by one, if count drops to zero then remove x and collect car(x) and cdr(x) */
void collect(L x) {
 I i; L y;
 while (1) {
  if (ref[(i = ord(x))/2] & FREE) {                     /* detect double free (which will or should never happen) */
   printf("\n\e[31;1mdouble free %u\e[m\t",i);
   err(4,nil);
  }
  if (ref[i/2] & SCC) {                                 /* if this is an SCC cell pair to collect */
   i = ref[i/2] & ~SCC;                                 /* then get the SCC representative identified by i */
   if (!(ref[i/2] & FREE) && --ref[i/2]) break;         /* if the representative was deleted or its ref drops to zero */
   return delscc(SCC|i,x);                              /* then delete the entire SCC and gc its branches */
  }
  if (--ref[i/2]) break;                                /* if ref count drops to zero (of a non-SCC cell pair x) */
  LOG(x,"\n\e[35mfree %u\e[m\t",i);
  del(i);                                               /* then delete the cell pair to reuse */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) collect(y);
   else x = y;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) return;
 }
 LOG(x,"\n\e[35m--#%u=%u\e[m\t",i,ref[i/2]);
}

/* rebuild ref count by incrementing the ref counts of all cells reachable from cell pair x */
void mark(L x) {
 L y; I i;
 while (!ref[(i = ord(x))/2]++) {                       /* increment ref count, but recurse at most once on x */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) mark(y);
   else x = y;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) return;
 }
}
/* sweep unused cells after mark() into the free cell pair list, shrink the atom heap when possible */
void sweep() {
 I i; for (hp = 0,i = 0; i < N; ++i) if (ref[i/2] && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 if (hp) hp += strlen(A+hp)+1;
 for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2) if (ref[i/2]) lomem(i); else del(i);
}
/* rebuild memory to retain the global environment env and delete everything else */
void rebuild() {
 I k = fn;
#if DEBUG
 I i,r[N/2];
 memcpy(r,ref,sizeof(ref));
#endif
 memset(ref,0,sizeof(ref));
 mark(env);
 sweep();
#if DEBUG                                               /* report on memory management when debugging is enabled */
 for (i = 0; i < N/3; ++i) {
  if (!(ref[i] & FREE) && (r[i] & FREE))
   LOG(cell[i+1],"\n\e[31;1muse after free ref[%u] = %u\e[m\t",i,ref[i]),LOG(cell[2*i],"\t");
  else if ((ref[i] & FREE) && !(r[i] & FREE))
   LOG(cell[i+1],"\n\e[31;1mnot freed pair ref[%u] = %u\e[m\t",i,r[i]),LOG(cell[2*i],"\t");
  else if (!(ref[i] & FREE) && !(r[i] & FREE) && ref[i] != r[i])
   LOG(cell[i+1],"\n\e[31;1mref[%u] want %u have %u\e[m\t",i,ref[i],r[i]),LOG(cell[2*i],"\t");
 }
#endif
 if (k < fn) printf("\ncollected %u unused cells",2*(fn-k));
 xb = xp = NULL;                                        /* clear exception stack pointers */
}

/* detect SCC from origin cell[i] while visiting x, ignore paths to cell[k] */
I cyclic(I i,L x,I k) {
 if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
  I j = ord(x);
  if (i != j && !(ref[j/2] & SCC)) {
   L y = cell[j+1],z = j == k ? nil : cell[j];          /* y = car(x) and z = cdr(x) if not cell[k] */
   ref[i/2] |= MARK;
   if (cyclic(i,y,k)) ref[j/2] = SCC|i;                 /* car(x) is in the SCC identified by representative i */
   if (cyclic(i,z,k)) ref[j/2] = SCC|i;                 /* cdr(x) is in the SCC identified by representative i */
   ref[i/2] &= ~MARK;
  }
  if (i == j) {
   --ref[i/2];
   LOG(x,"\n\e[36m--#@%u=%u\e[m\t",i,ref[i/2]);
   return 1;                                            /* x is in the SCC identified by representative i */
  }
  if (ref[j/2] & MARK) return 0;                        /* ignore cycles that are not in the SCC */
  if (ref[j/2] == (SCC|i)) {
   LOG(x,"\n\e[36m%u @%u\e[m\t",j,i);
   return 1;                                            /* x is in the SCC identified by representative i */
  }
 }
 return 0;                                              /* x is not in an SCC */
}
/* if x is strongly connected, then all cyclic paths that go through x are in the SCC, ignore paths to cell[k] */
void scc(L x,I k) {
 I i = ord(x); L y = cell[i+1],z = i == k ? nil : cell[i];      /* y = car(x) and z = cdr(x) if not cell[k] */
 cyclic(i,y,k);
 cyclic(i,z,k);
}

/* section 16.1: replacing recursion with loops */
L evlis(L t,L e) {
 L s,*p = &s;
 rc(&s,nil);
 for (; T(t) == CONS; p = &CDR(*p),t = CDR(t)) *p = cons(eval(CAR(t),e),nil);
 if (T(t) == ATOM) *p = dup(assoc(t,e));
 rr(1);
 return s;
}

/* section 16.4: optimizing the lisp primitives */
L evarg(L *t,L *e,I *a) {
 L x;
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 x = car(*t); *t = cdr(*t);
 return *a ? dup(x) : eval(x,*e);
}
I isarg(L *t,L *e,I *a,L *x) {
 if (T(*t) == ATOM && !*a) *t = assoc(*t,*e),*a = 1;
 if (not(*t)) return 0;
 *x = car(*t); *t = cdr(*t);
 *x = *a ? dup(*x) : eval(*x,*e);
 return 1;
}

/* section 6 lisp primitives (optimized with evarg per section 16.4) */
L f_eval(L t,L *e) { I a = 0; L x,y; rc(&x,evarg(&t,e,&a)); y = eval(x,*e); rg(x); return y; }
L f_quote(L t,L *_) { return dup(car(t)); }
L f_cons(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return cons(x,evarg(&t,e,&a)); }
L f_car(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(car(x)); gc(x); return y; }
L f_cdr(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(cdr(x)); gc(x); return y; }
L f_add(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n += gc(x); return num(n); }
L f_sub(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n -= gc(x); return num(n); }
L f_mul(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n *= gc(x); return num(n); }
L f_div(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n /= gc(x); return num(n); }
L f_int(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); return n < 1e16 && n > -1e16 ? (long long)n : num(n); }
/* compare two values of any type, not only compare numbers (make it a total ordering) */
L f_lt(L t,L *e) {
 I a = 0; L x = gc(evarg(&t,e,&a)),y = gc(evarg(&t,e,&a));
 return (T(x) == ATOM && T(y) == ATOM ? strcmp(A+ord(x),A+ord(y)) < 0 :
  x == x && y == y ? x < y :                    /* x == x is false when x is NaN i.e. a tagged Lisp expression */
  T(x) < T(y) || (T(x) == T(y) && ord(x) < ord(y))) ? tru : nil;
}
L f_eq(L t,L *e) { I a = 0; L x = gc(evarg(&t,e,&a)); return equ(x,gc(evarg(&t,e,&a))) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = gc(evarg(&t,e,&a)); return T(x) == CONS ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; while (isarg(&t,e,&a,&x) && not(x)) continue; return x; }
L f_and(L t,L *e) { I a = 0; L x = tru,y = nil; while (isarg(&t,e,&a,&x)) { gc(y); y = x; if (not(x)) break; } return x; }
L f_not(L t,L *e) { I a = 0; return not(gc(evarg(&t,e,&a))) ? tru : nil; }
L f_cond(L t,L *e) { while (not(gc(eval(car(car(t)),*e)))) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { return car(cdr(not(gc(eval(car(t),*e))) ? cdr(t) : t)); }
L f_leta(L t,L *e) {
 for (; let(t); t = CDR(t))
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),eval(car(CDR(CAR(t))),*e),*e);
  else err(2,CAR(t));                           /* bound variable must be an atom, to prevent GC issues when not an atom */
 return car(t);
}
L f_lambda(L t,L *e) { return closure(dup(car(t)),dup(car(cdr(t))),equ(*e,env) ? nil : dup(*e)); }
/* defining a global symbol garbage-collects unreachable definitions when redefined */
L f_define(L t,L *e) {
 L d = *e,v = car(t),x;
 if (T(v) != ATOM) return err(2,v);             /* bound variable must be an atom, to prevent GC issues when not an atom */
 x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(CAR(d)))) d = CDR(d);
 if (T(d) != CONS) env = pair(v,x,env);
 else {
  gc(CDR(CAR(d)));
  CDR(CAR(d)) = x;
  printf("redefined ");
 }
 return v;
}

/* section 11: additional Lisp primitives (optimized with evarg per section 16.4) */
L f_assoc(L t,L *e) { I a = 0; L d,x,v = gc(evarg(&t,e,&a)); rc(&d,evarg(&t,e,&a)); x = dup(assoc(v,d)); rg(d); return x; }
L f_env(L _,L *e) { return dup(*e); }
L f_let(L t,L *e) {
 L d = *e;
 for (; let(t); t = CDR(t))
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),eval(car(CDR(CAR(t))),d),*e);
  else err(2,CAR(t));                           /* bound variable must be an atom, to prevent GC issues when not an atom */
 return car(t);
}
L f_letreca(L t,L *e) {
 I i,k;
 for (; let(t); t = CDR(t)) {
  if (T(CAR(t)) == CONS && T(CAR(CAR(t))) == ATOM) *e = pair(CAR(CAR(t)),nil,*e);
  else err(2,CAR(t));                           /* bound variable must be an atom, to prevent GC issues when not an atom */
  k = ref[(i = ord(*e))/2];
  CDR(CAR(*e)) = eval(car(CDR(CAR(t))),*e);
  if (ref[i/2] > k) scc(*e,i);                  /* use of *e detected in a CLOS: mark strongly connected component */
 }
 return car(t);
}
L f_letrec(L t,L *e) {
 I i,k;L s,d,*p;
 for (s = t,d = *e,p = &d; let(s); s = CDR(s),p = &CDR(*p))
  if (T(CAR(s)) == CONS && T(CAR(CAR(s))) == ATOM) *p = pair(CAR(CAR(s)),nil,*e);
  else err(2,CAR(s));                           /* bound variable must be an atom, to prevent GC issues when not an atom */
 k = ref[(i = ord(d))/2];
 for (*e = d; let(t); t = CDR(t),i = ord(d),d = CDR(d)) CDR(CAR(d)) = eval(car(CDR(CAR(t))),*e);
 if (ref[ord(*e)/2] > k) scc(*e,i);             /* use of *e detected in a CLOS: mark strongly connected component */
 return car(t);
}
L f_setq(L t,L *e) {
 L d = *e,v = car(t),x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(CAR(d)))) d = CDR(d);
 if (T(d) != CONS) err(2,v);
 gc(CDR(CAR(d)));
 return CDR(CAR(d)) = dup(x);
}
L f_setcar(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = dup(evarg(&t,e,&a)); gc(CAR(p)); CAR(p) = x; rg(p);
 return x;
}
L f_setcdr(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = dup(evarg(&t,e,&a)); gc(CDR(p)); CDR(p) = x; rg(p);
 return x;
}
L f_macro(L t,L *_) { return macro(dup(car(t)),dup(car(cdr(t)))); }
L f_print(L t,L *e) { I a = 0; L x; for (; isarg(&t,e,&a,&x); gc(x)) print(out,x); return nil; }
L f_println(L t,L *e) { f_print(t,e); fputc('\n',out); return nil; }

/* ++ new: atomize (stringify) x */
L f_atomize(L t,L *e) {
 I i = hp,k; L s,*p = &s;
 rc(&s,nil);                                    /* register s to garbage collect when an error is caught by f_catch */
 for (; T(t) == CONS; t = CDR(t),p = &CDR(*p)) *p = cons(T(CAR(t)) == ATOM ? CAR(t) : eval(CAR(t),*e),nil);
 if (T(t) != NIL) *p = t;
 k = atomize(s,NULL);                           /* the atom string length k, to hold atomized list of arguments */
 if ((hp += k+1) > lp<<3) err(4,nil);           /* ERR 4 if the heap space is not large enough */
 atomize(s,A+i);                                /* store the atomized arguments on the heap */
 rg(s);                                         /* deregister s and garbage collect it */
 return box(ATOM,i);
}

/* ++ updated: read from file with optional pathname argument converted using atomize */
L f_read(L t,L *e) {
 L x; char c = see;
 if (T(t) != NIL) {
  x = f_atomize(t,e);
  if (ld >= sizeof(in)/sizeof(*in) || !(in[ld++] = fopen(A+ord(x),"r"))) err(5,x);
 }
 see = ' '; x = Read(); see = c;
 if (T(t) != NIL) fclose(in[--ld]);
 return x;
}

/* section 12: adding readline with history ++ updated: support multiple loads and nested loads */
L f_load(L t,L *e) {
 I j,k = ld; L s,v = nil;
 while (T(t) == CONS) {
  s = CDR(t); CDR(t) = nil;                     /* temporarily set cdr(t) to nil */
  v = f_atomize(t,e);                           /* atomize one argument */
  t = CDR(t) = s;                               /* restore cdr(t) and visit next argument */
  if (ld >= sizeof(in)/sizeof(*in) || !(in[ld++] = fopen(A+ord(v),"r"))) err(5,v);
 }
 for (j = ld-1; j > k; --j,++k) { FILE *f = in[j]; in[j] = in[k]; in[k] = f; }  /* reverse the in[] additions */
 return v;
}

/* section 13: execution tracing */
L f_trace(L t,L *_) { tr = not(t) ? !tr : (I)num(car(t)); return num(tr); }

/* section 14: error handling and exceptions */
L f_catch(L t,L *e) {
 I i; L x,**saved[2] = {xb,xp};                 /* save old xb and xp exception stack pointers */
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 if (!xp) xp = xstk;                            /* set exception stack pointer xp if not set */
 xb = xp;                                       /* set base stack pointer xb for evals after f_catch */
 if ((i = setjmp(jb)) == 0) x = eval(car(t),*e);
 memcpy(jb,savedjb,sizeof(jb));
 xb = saved[0]; xp = saved[1];                  /* restore xb and xp exception stack pointers */
 return i == 0 ? x : i == 4 ? err(4,nil) : cons(atom("ERR"),i);
}
L f_throw(L t,L *_) { return err(num(car(t)),nil); }

/* section 16.5: tail-call optimization */
L f_progn(L t,L *e) {
 for (; let(t); t = CDR(t)) gc(eval(CAR(t),*e));
 return car(t);
}
L f_while(L t,L *e) {
 L s,x = nil,y;
 rc(&y,nil);                                    /* register y to garbage collect when an error is caught by f_catch */
 while (!not(gc(eval(car(t),*e))))
  for (s = cdr(t); T(s) == CONS; s = CDR(s),gc(y),y = x) x = eval(CAR(s),*e);
 rr(1);                                         /* deregister y */
 return x;
}

/* ++ new: write the output of print/ln of a sequence of expressions to a file, append if the filename starts with a '+' */
L f_writeto(L t,L *e) {
 L x = cons(dup(car(t)),nil),y = nil,v = f_atomize(x,e); I i,k = *(A+ord(v)) == '+';
 FILE *savedout = out;                          /* save old out */
 jmp_buf savedjb;                               /* save old jmp buf */
 memcpy(savedjb,jb,sizeof(jb));
 gc(x);                                         /* garbage collect list x we atomized as v */
 if (!(out = fopen(A+ord(v)+k,k ? "a" : "w"))) err(5,v);        /* open file for writing or apending as new out */
 if ((i = setjmp(jb)) == 0) y = eval(f_progn(cdr(t),e),*e);     /* catch error in eval of progn of the rest of args */
 fclose(out);                                   /* close out */
 out = savedout;                                /* restore old out */
 memcpy(jb,savedjb,sizeof(jb));                 /* restore old jmp buf */
 if (i) { gc(y); longjmp(jb,i); }               /* re-throw error after garbage collecting y */
 return y;
}

/* ++ new: the type of an expression, 0 = number, 1 = atom, 2 = primitive, 3 = pair, 4 = closure, 5 = macro, 6 = nil */
L f_type(L t,L *e) { I a = 0; L x = gc(evarg(&t,e,&a)); return (T(x) & ATOM) >= ATOM ? (T(x)) - ATOM + 1 : 0; }

L f_quit(L t,L *e) { I a = 0; L x; exit(isarg(&t,e,&a,&x) ? (int)num(x) : 0); }

struct { const char *s; L (*f)(L,L*); short t; } prim[] = {
 {"eval",    f_eval,   0}, /* no longer tail recursive to implement gc */
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
 {"letrec",  f_letrec, 1},
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
 {"atomize", f_atomize,0},
 {"write-to",f_writeto,0},
 {"type",    f_type,   0},
 {"quit",    f_quit,   0},
 {0}};

/* section 13: tracing (trace 1) with colorful output, to wait on ENTER (trace 2), with memory dump (trace 3) */
void trace(L y,L x,L e) {
 if (tr > 2 && !equ(e,env)) { printf("\n\e[35mENV: \e[33m"); print(stdout,e); printf("\e[m"); }
 printf("\n\e[32m%u \e[33m",lp); print(stdout,y); printf("\e[36m => \e[33m"); print(stdout,x); printf("\e[m\t");
 if (tr > 1) while (getchar() >= ' ') continue;
}

/* section 16.2/3/4: tail-call optimization */
L eval(L x,L e) {
 I a; L f,v,d,y,g,h;
 /* if f_catch-ing then register 5 variables to track and garbage collect when an error is caught by f_catch */
 rc(&d,nil); rc(&e,dup(e)); rc(&f,nil); rc(&g,nil); rc(&h,nil);
 /* we dup(e) the environment to extend with locals and formal arguments, then gc(e) all of them afterwards */
 while (1) {
  /* copy x to y to output y => x when tracing is enabled */
  y = x;
  /* if ix is an atom, then return its value; if x is not an application list (it is constant), then return x */
  if (T(x) == ATOM) { x = dup(assoc(x,e)); break; }
  if (T(x) != CONS) { x = dup(x); break; }
  /* save g = old f to garbage collect, evaluate f in the application (f . x) and get the list of arguments x */
  g = f; f = eval(CAR(x),e); x = CDR(x);
  if (T(f) == PRIM) {
   /* apply Lisp primitive to argument list x, return value in x */
   x = prim[ord(f)].f(x,&e);
   /* garbage collect g = old f */
   gc(g); g = nil;
   /* if tail-call then continue evaluating x, otherwise return x */
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) == MACR) {
   /* bind macro f variables v to the given arguments literally (i.e. without evaluating the arguments) */
   for (d = dup(env),v = CAR(f); T(v) == CONS; v = CDR(v),x = cdr(x)) d = pair(CAR(v),dup(car(x)),d);
   if (T(v) == ATOM) d = pair(v,dup(x),d);
   /* expand macro f, then continue evaluating the expanded x */
   x = eval(CDR(f),d);
   /* garbage collect bindings d, garbage collect g = old f and old macro body h, save macro body h = x to gc later */
   gc(d); d = nil; gc(g); g = nil; gc(h); h = x;
   continue;
  }
  if (T(f) != CLOS) return err(3,f);
  /* get the list of variables v of closure f and its local environment d (use global env when nil) */
  v = car(CAR(f)); d = dup(CDR(f));
  if (T(d) == NIL) d = dup(env);
  /* bind closure f variables v to the evaluated argument values */
  for (a = 0; T(v) == CONS; v = CDR(v)) d = pair(CAR(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? dup(x) : evlis(x,e),d);
  /* next, evaluate body x of closure f in environment e = d while keeping f in memory as long as x */
  x = cdr(CAR(f));
  /* discard copy of the old environment e to use new environment d */
  gc(e); e = d; d = nil;
  /* garbage collect closure g = old f with old body x, garbage collect old macro body h */
  gc(g); g = nil; gc(h); h = nil;
  if (tr) trace(y,x,e);
 }
 if (tr) trace(y,x,e);
 /* garbage collect environment e, closure f, macro body h */
 gc(e); gc(f); gc(h);
 /* deregister 5 variables, if registered, without gc'ing them */
 rr(5);
 return x;
}

/* section 12: adding readline with history */
void look() {
 while (ld) {
  int c;
  if (!in[--ld]) err(5,nil);
  see = c = getc(in[ld++]);
  if (c != EOF) return;
  fclose(in[--ld]);
  see = '\n';
 }
 if (see == '\n') {
  if (line) { ptr = line; line = NULL; free(ptr); }
  while (!(ptr = line = readline(ps))) freopen("/dev/tty","r",stdin);
  add_history(line);
  snprintf(ps,sizeof(ps),PS2);
 }
 if (!(see = *ptr++)) see = '\n';
}
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }
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
L list() {
 L t,*p;
 for (t = nil,p = &t; ; *p = cons(parse(),nil),p = &CDR(*p)) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = Read(),scan(),t;
 }
}
L tick() {
 L t,*p;
 if (*buf == ',') return Read();
 if (*buf == '\'') return scan(),cons(atom("list"),cons(cons(atom("quote"),cons(atom("quote"),nil)),cons(tick(),nil)));
 if (*buf == '"') return parse();
 if (*buf != '(') return cons(atom("quote"),cons(parse(),nil));
 for (t = cons(atom("list"),nil),p = &t; ; p = &CDR(*p),*p = cons(tick(),nil)) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return scan(),t = cons(atom("append"),cons(t,cons(tick(),nil))),scan(),t;
 }
}
L parse() {
 L n; int i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(Read(),nil));
 if (*buf == '`') return scan(),tick();
 if (*buf == '"') return cons(atom("quote"),cons(atom(buf+1),nil));
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
 if (T(x) == NIL) strcpy(buf," ");
 else if (T(x) == ATOM) strcpy(buf,A+ord(x));
 else snprintf(buf,sizeof(buf),"%.10lg",x);
 if (a) strcpy(a,buf);
 return strlen(buf);
}

/* section 14: error handling and exceptions */
void stop(int i) { if (line) err(6,nil); else abort(); }

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; printf("tinylisp-extras-gc");
 sweep(); /* sweep all cells to the free list (since all ref[] are zero and xb = xp = NULL) */
 nil = box(NIL,0); atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 in[ld++] = fopen((argc > 1 ? argv[1] : "common.lisp"),"r");
 using_history();
 signal(SIGINT,stop);
 if ((i = setjmp(jb)) > 0) {
  while (ld) if (in[--ld]) fclose(in[ld]);
  printf("ERR %u",i);
 }
 out = stdout;
 while (1) {
  L x;
  rebuild();
  putchar('\n'); snprintf(ps,sizeof(ps),PS1,2*fn-hp/8);
  print(out,gc(eval(x = Read(),env)));
  gc(x);
 }
}
