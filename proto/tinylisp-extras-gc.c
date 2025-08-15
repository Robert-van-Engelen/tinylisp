/* tinylisp-extras-gc.c with the article's extras and ref count garbage collection by Robert A. van Engelen 2025 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* DEBUG: enable memory management activity logging (=1 enable, =2 include Lisp expression dumps) */
#if DEBUG == 0
# define LOG(x,...) 0
#elif DEBUG == 1
# define LOG(x,...) printf(__VA_ARGS__)
#else
# define LOG(x,...) (printf(__VA_ARGS__),printf("\e[33m"),print(x),printf("\e[m\t"))
#endif

/* we only need two types to implement a Lisp interpreter:
        I      unsigned integer (either 16 bit, 32 bit or 64 bit unsigned)
        L      Lisp expression (double with NaN boxing)
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
#define I unsigned
#define L double

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) *(unsigned long long*)&x>>48

/* address of the atom heap is at the bottom of the cell pool */
#define A (char*)cell

/* number of cells for the shared pool and atom heap, increase N as desired */
#define N 8192

/* section 12: adding readline with history */
#include <readline/readline.h>
#include <readline/history.h>
FILE *in = NULL;
char buf[40],see = ' ',*ptr = "",*line = NULL,ps[20];

/* forward proto declarations */
L eval(L,L),Read(),parse(),err(I,L),gc(L); void collect(L),print(L);

/* section 4: constructing Lisp expressions (using a cell pool managed with reference count garbage collection) */
/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   fp: free cell pairs list pointer, fp->ref[fp]->ref[ref[fp]]->...->0 forms a linked list of free cell pair indices
   lp: pointer to the lowest allocated and used cell pair in cell[]
   fn: number of free cell cons pairs in cell[] (not taking atoms stored in cell[] into account)
   tr: tracing off (0), on (1), wait on ENTER (2), dump and wait (3)
   safety invariant: hp < lp<<3 */
I hp = 0,fp = N-2,lp = N-2,fn = N/2,tr = 0;
/* ref[N] array with ref count of a used cell pair or ref to next free cell pair in the free list */
I ref[N];
/* atom, primitive, cons, closure and nil tags for NaN boxing */
enum { ATOM = 0x7ff8,PRIM = 0x7ff9,CONS = 0x7ffa,CLOS = 0x7ffb,MACR = 0x7ffc,NIL = 0x7ffd };
/* cell[N] pool of allocatable Lisp expressions shared by the atom heap */
L cell[N];
/* Lisp constant expressions () (nil), #t, and the global environment env */
L nil,tru,env;
/* NaN-boxing specific functions:
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
L box(I t,I i) { L x; *(unsigned long long*)&x = (unsigned long long)t<<48|i; return x; }
I ord(L x) { return *(unsigned long long*)&x; }
L num(L n) { return n; }
I equ(L x,L y) { return *(unsigned long long*)&x == *(unsigned long long*)&y; }
/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp && (hp += strlen(strcpy(A+i,s))+1) > lp<<3) err(4,nil);
 return box(ATOM,i);
}

/* section 14: error handling and exceptions
   ERR 1: not a pair
   ERR 2: unbound symbol
   ERR 3: cannot apply
   ERR 4: out of memory
   ERR 5: program stopped */
#include <setjmp.h>
#include <signal.h>
/* max number of nested eval() calls between f_catch and f_throw */
#define K 1024
/* exception stack and pointers to track "lost" variables between f_catch() and err() to garbage collect */
L *xstk[5*K],**xb = NULL,**xp = NULL;
jmp_buf jb;
/* throw an error, if f_catch handler is used (xb < xp are not NULL) then garbage collect "lost" variables */
L err(I i,L x) {
 const char *msg[5] = {"not a pair","unbound","cannot apply","out of memory","stopped"};
 if (xp ? tr : i >= 1 && i <= 5) {
  printf("\n\e[31;1mERR %u: ",i);
  if (T(x) != NIL) print(x);
  printf(" %s\e[m\n",i >= 1 && i <= 5 ? msg[i-1] : "");
 }
 while (xp != xb) gc(**--xp);
 longjmp(jb,i);
}

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(L x,L y) {
 I i = fp;                                              /* get a cell pair from the free pool */
 if (i < lp) lp = i;                                    /* update lowest cell pair used */
 if (hp > lp<<3) err(4,nil);
 fp = ref[i]; ref[i] = 1; --fn;                         /* claim the cell pair from the free pool */
 ref[i+1] = 0;                                          /* clear markers in upper ref */
 cell[i+1] = x; cell[i] = y;                            /* store car x and cdr y */
 LOG(box(CONS,i),"\n\e[32mcons => %u\e[m\t",i);
 return box(CONS,i);
}
/* return the car of a pair or throw err(1) if not a pair */
L car(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)+1] : err(1,p); }
/* return the cdr of a pair or throw err(1) if not a pair */
L cdr(L p) { return T(p) == CONS || T(p) == CLOS || T(p) == MACR ? cell[ord(p)] : err(1,p); }
/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
/* construct a closure, returns a NaN-boxed CLOS */
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,e))); }
/* construct a macro, returns a NaN-boxed MACR */
L macro(L v,L x) { return box(MACR,ord(cons(v,x))); }
/* look up a symbol in an environment, return its value or throw err(2) if not found */
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e); return T(e) == CONS ? cdr(car(e)) : err(2,v); }
/* not(x) is nonzero if x is the Lisp () empty list */
I not(L x) { return T(x) == NIL; }
/* let(x) is nonzero if x has more than one item, used by let* */
I let(L x) { return !not(x) && !not(cdr(x)); }

/* duplicate expression x: if x is a pair then increment its ref count by one */
L dup(L x) {
 if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
  ++ref[ord(x)];
  LOG(x,"\n\e[32m++ref[%u] == %u\e[m\t",ord(x),ref[ord(x)]);
 }
 return x;
}

/* garbage collect: if x is a pair then collect pair x by decrementing its ref count, remving it if count drops to 0 */
L gc(L x) { if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) collect(x); return x; }

/* garbage collection upper ref markers and masks */
const I FREE = (I)~0UL,VISIT = ~(FREE>>1),MARKY = VISIT>>1,MARKZ = MARKY>>1,MARK = MARKY|MARKZ,MASK = ~(VISIT|MARK);

/* recursively mark all cyclic paths in the strongly connected component x, origin cell[i], ignore paths to cell[k] */
I cyclic(I i,L x,I k) {
 if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
  I j = ord(x);
  if (i != j && ref[j+1] == 0) {
   L y = cell[j+1],z = j == k ? nil : cell[j];                  /* y = car(x) and z = cdr(x) if not cell[k] */
   if (cyclic(i,y,k)) ref[j+1] |= VISIT | MARKY;                /* mark cyclic paths through car(x) */
   if (cyclic(i,z,k)) ref[j+1] |= VISIT | MARKZ;                /* mark cyclic paths through cdr(x) */
  }
  if (i == j || (ref[j+1] & VISIT)) {
   ++ref[j+1];                                                  /* increase cyclic ref count through x by one */
   LOG(x,"\n\e[36m++cycle[%u] == %u\e[m\t",j,ref[j+1] & MASK);
   return 1;                                                    /* path through x is cyclic */
  }
 }
 return 0;                                                      /* path through x is not cyclic */
}
/* unvisit the strongly connected component x, removes all VISIT marks */
void leave(L x) {
 I i,k; L y;
 while ((k = ref[(i = ord(x))+1]) & VISIT) {
  x = cell[i]; y = cell[i+1];
  ref[i+1] &= ~VISIT;
  if (k & MARKY) {
   if (k & MARKZ) leave(y);
   else x = y;
  }
 }
}
/* if x is strongly connected, then mark all cyclic paths through x, ignore paths to cell[k] */
void scc(L x,I k) {
 I i = ord(x); L y = cell[i+1],z = i == k ? nil : cell[i];      /* y = car(x) and z = cdr(x) if not cell[k] */
 if (cyclic(i,y,k)) ref[i+1] |= VISIT | MARKY;                  /* mark cyclic paths through car(x) */
 if (cyclic(i,z,k)) ref[i+1] |= VISIT | MARKZ;                  /* mark cyclic paths through cdr(x) */
 leave(x);
}

/* returns nonzero if strongly connected component through all cyclic paths from cell pair x has become isolated */
I isocyclic(L x) {
 I i,k; L y;
 while (!((k = ref[(i = ord(x))+1]) & VISIT)) {
  LOG(x,"\n\e[36mcycle[%u] == %u %c= \e[35m%u\e[m\t",i,k & MASK,(k & MASK) == ref[i] ? '=' : '!',ref[i]);
  if ((k & MASK) < ref[i]) return 0;
  ref[i+1] |= VISIT;
  x = cell[i]; y = cell[i+1];                           /* y = car(x) and x = cdr(x) */
  if (k & MARKY) {
   if (!(k & MARKZ)) x = y;                             /* only recurse on car(x) */
   else if (!isocyclic(y)) return 0;                    /* recurse on both car(x) and cdr(x) */
  }
 }
 return 1;
}
/* returns nonzero if x is a strongly connected component that has become isolated and can be garbage collected */
I isoscc(L x) {
 I i = ord(x),k;
 if (!(ref[i+1] & MARK)) return 0;                      /* x is not a strongly connected component */
 k = isocyclic(x);
 leave(x);
 return k;
}
/* garbage collect strongly connected component x */
void gcscc(L x) {
 I i,k; L y;
 while ((k = ref[(i = ord(x))+1]) != FREE) {            /* repeat until all free */
  LOG(x,"\n\e[36mfree cyclic %u\e[m\t",i);
  ref[i] = fp; fp = i; ++fn;                            /* reclaim cell pair x in the free pool */
  ref[i+1] = FREE;                                      /* mark it free */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) {
    if (!(k & MARKY)) collect(y);                       /* only cdr(x) is part of the SCC, car(x) is a pair */
    else if (k & MARKZ) gcscc(y);                       /* both car(x) and cdr(x) are part of the SCC */
    else collect(x),x = y;                              /* only car(x) is part of the SCC, cdr(x) is a pair */
   }
   else x = y;                                          /* only car(x) is part of the SCC */
  }
 }
}

/* collect pair x: decrement ref count by one, if count drops to 0 then remove x and collect car(x) and cdr(x) */
void collect(L x) {
 I i; L y;
 while (1) {
  i = ord(x);
  if (ref[i+1] == FREE) {                               /* detect double free (should never happen!) */
   printf("\n\e[31;1mdouble free %u\e[m\t",i);
   err(4,nil);
  }
  if (--ref[i]) {                                       /* decrement ref count by one, if not yet zero then ... */
   if (isoscc(x)) return gcscc(x);                      /* if it is an isolated SCC then garbage collect it */
   break;                                               /* else do nothing */
  }
  LOG(x,"\n\e[35mfree %u -> %u%s\e[m\t",i,fp,ref[i+1] ? " \e[36mcycle" : "");
  ref[i] = fp; fp = i; ++fn;                            /* reclaim cell pair x in the free pool */
  ref[i+1] = FREE;                                      /* mark it free */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) collect(y);
   else x = y;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) return;
 }
 LOG(x,"\n\e[35m--ref[%u] == %u%s\e[m\t",i,ref[i],ref[i+1] ? " \e[36mcycle" : "");
}

/* register x with initial value y to collect with rg(x) or in a f_catch exception handler when an error occurred */
L rc(L *x,L y) { return !xp ? (*x = y) : xp >= xstk+5*K ? err(4,nil) : (*(*xp++ = x) = y); }

/* remove x from catch-throw registry and garbage collect */
L rg(L x) { if (xp) --xp; return gc(x); }

/* remove n registrations without garbage collecting them */
void rr(I k) { if (xp) xp -= k; }

/* mark to rebuild ref count by marking used cells recursively and by incrementing ref counts */
void mark(L x) {
 L y; I i;
 while (ref[i = ord(x)]++ == 0) {                       /* increment ref count, but recurse at most once on x */
  x = cell[i]; y = cell[i+1];                           /* recurse on y = car(x) and x = cdr(x) */
  if (T(y) == CONS || T(y) == CLOS || T(y) == MACR) {
   if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) mark(y);
   else x = y;
  }
  else if (T(x) != CONS && T(x) != CLOS && T(x) != MACR) return;
 }
}

/* sweep unused cells after mark() into the free list, also shrink the atom heap when possible */
void sweep() {
 I i;
 for (hp = 0,i = 0; i < N; ++i) if (ref[i&~1] > 0 && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
 for (lp = N-2,fn = 1,fp = 0,i = 2; i < N; i += 2)
  if (ref[i] == 0) { ref[i] = fp; fp = i; ++fn; }       /* reclaim cell pair at i in the free pool */
  else if (lp > i) lp = i;                              /* find lowest cell pair used */
}

/* rebuild memory to retain the global environment env and delete everything else */
void rebuild() {
 I i,k = fn;
 for (i = 0; i < N; i += 2) { ref[i+1] = ref[i]; ref[i] = 0; }
 mark(env);
 sweep();
 for (i = 0; i < N; i += 2) {                           /* report on memory management when debugging is enabled */
  if (ref[i] != ref[i+1]) {
   if (ref[i] < hp/8 && ref[i+1] < hp/8)
    LOG(cell[i+1],"\n\e[31;1mref[%u] want %u have %u\e[m\t",i,ref[i],ref[i+1]),LOG(cell[i],"\t");
   else if (ref[i] < hp/8)
    LOG(cell[i+1],"\n\e[31;1muse after free ref[%u] = %u\e[m\t",i,ref[i]),LOG(cell[i],"\t");
   else if (ref[i+1] < hp/8)
    LOG(cell[i+1],"\n\e[31;1mnot freed pair ref[%u] = %u\e[m\t",i,ref[i+1]),LOG(cell[i],"\t");
  }
 }
 if (k < fn) printf("\ncollected %u unused cells",2*(fn-k));
 for (i = 0; i < N; i += 2) ref[i+1] = 0;               /* clear all cell pair upper ref for letrec cycle detection */
 for (i = fp; i > 0; i = ref[i]) ref[i+1] = FREE;       /* mark free cell pair upper ref for double free detection */
 xb = xp = NULL;                                        /* clear exception stack pointers */
}

/* section 16.1: replacing recursion with loops (combining d = pair(v,evlis(t,e),d) into one for gc) */
L evlis(L v,L t,L e,L *d) {
 L *p;
 for (*d = pair(v,nil,*d),p = &cell[ord(car(*d))]; T(t) == CONS; p = &cell[ord(*p)],t = cdr(t))
  *p = cons(eval(car(t),e),nil);
 if (T(t) == ATOM) *p = dup(assoc(t,e));
 return *d;
}

/* section 16.4: optimizing the lisp primitives */
L evarg(L *t,L *e,I *a) {
 L x;
 if (T(*t) == ATOM) *t = assoc(*t,*e),*a = 1;
 x = car(*t); *t = cdr(*t);
 return *a ? dup(x) : eval(x,*e);
}

/* section 6 lisp primitives (optimized with evarg per section 16.4) */
L f_eval(L t,L *e) { I a = 0; L x,y; rc(&x,evarg(&t,e,&a)); y = eval(x,*e); rg(x); return y; }
L f_quote(L t,L *_) { return dup(car(t)); }
L f_cons(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return cons(x,evarg(&t,e,&a)); }
L f_car(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(car(x)); gc(x); return y; }
L f_cdr(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(cdr(x)); gc(x); return y; }
L f_add(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); while (!not(t)) n += gc(evarg(&t,e,&a)); return num(n); }
L f_sub(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); while (!not(t)) n -= gc(evarg(&t,e,&a)); return num(n); }
L f_mul(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); while (!not(t)) n *= gc(evarg(&t,e,&a)); return num(n); }
L f_div(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); while (!not(t)) n /= gc(evarg(&t,e,&a)); return num(n); }
L f_int(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); return n<1e16 && n>-1e16 ? (long long)n : n; }
L f_lt(L t,L *e) { I a = 0; L n = gc(evarg(&t,e,&a)); return n - gc(evarg(&t,e,&a)) < 0 ? tru : nil; }
L f_eq(L t,L *e) { I a = 0; L x = gc(evarg(&t,e,&a)); return equ(x,gc(evarg(&t,e,&a))) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = gc(evarg(&t,e,&a)); return T(x) == CONS ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; while (!not(t) && not(x = evarg(&t,e,&a))) gc(x); return x; }
L f_and(L t,L *e) { I a = 0; L x = tru; while (!not(t) && !not(x = evarg(&t,e,&a))) gc(x); return x; }
L f_not(L t,L *e) { I a = 0; return not(gc(evarg(&t,e,&a))) ? tru : nil; }
L f_cond(L t,L *e) { while (!not(t) && not(gc(eval(car(car(t)),*e)))) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { return car(cdr(not(gc(eval(car(t),*e))) ? cdr(t) : t)); }
L f_leta(L t,L *e) { for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),*e),*e); return car(t); }
L f_lambda(L t,L *e) { return closure(dup(car(t)),dup(car(cdr(t))),equ(*e,env) ? nil : dup(*e)); }

/* redefine f_define to garbage collect unreachable definitions when redefined */
L f_define(L t,L *e) {
 L d = *e,v = car(t),x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(car(d)))) d = cdr(d);
 if (T(d) != CONS) env = pair(v,x,env);
 else {
  gc(cell[ord(car(d))]);
  cell[ord(car(d))] = x;
  printf("redefined symbol ");
 }
 return v;
}

/* section 11: additional Lisp primitives (optimized with evarg per section 16.4) */
L f_assoc(L t,L *e) { I a = 0; L d,x,v = gc(evarg(&t,e,&a)); rc(&d,evarg(&t,e,&a)); x = dup(assoc(v,d)); rg(d); return x; }
L f_env(L _,L *e) { return dup(*e); }
L f_let(L t,L *e) {
 L d = *e;
 for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),d),*e);
 return car(t);
}
L f_letreca(L t,L *e) {
 I i,k;
 for (; let(t); t = cdr(t)) {
  *e = pair(car(car(t)),nil,*e);
  k = ref[i = ord(*e)];
  cell[ord(car(*e))] = eval(car(cdr(car(t))),*e);
  if (ref[i] > k) scc(*e,i);                    /* use of *e detected in a CLOS: mark strongly connected component */
 }
 return car(t);
}
L f_letrec(L t,L *e) {
 I i,k;L s,d,*p;
 for (s = t,d = *e,p = &d; let(s); s = cdr(s),p = &cell[ord(*p)]) *p = pair(car(car(s)),nil,*e);
 k = ref[ord(d)];
 for (*e = d; let(t); t = cdr(t),i = ord(d),d = cdr(d)) cell[ord(car(d))] = eval(car(cdr(car(t))),*e);
 if (ref[ord(*e)] > k) scc(*e,i);               /* use of *e detected in a CLOS: mark strongly connected component */
 return car(t);
}
L f_setq(L t,L *e) {
 L d = *e,v = car(t),x = eval(car(cdr(t)),d);
 while (T(d) == CONS && !equ(v,car(car(d)))) d = cdr(d);
 if (T(d) != CONS) err(2,v);
 gc(cell[ord(car(d))]);
 return cell[ord(car(d))] = dup(x);
}
L f_setcar(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = dup(evarg(&t,e,&a)); gc(cell[ord(p)+1]); cell[ord(p)+1] = x; rg(p);
 return x;
}
L f_setcdr(L t,L *e) {
 I a = 0; L x,p;
 rc(&p,evarg(&t,e,&a));
 if (T(p) != CONS) err(1,p);
 x = dup(evarg(&t,e,&a)); gc(cell[ord(p)]); cell[ord(p)] = x; rg(p);
 return x;
}
L f_macro(L t,L *_) { return macro(dup(car(t)),dup(car(cdr(t)))); }
L f_read(L t,L *_) { L x; char c = see; see = ' '; x = Read(); see = c; return x; }
L f_print(L t,L *e) { I a = 0; L x; for (; !not(t); gc(x)) print(x = evarg(&t,e,&a)); return nil; }
L f_println(L t,L *e) { f_print(t,e); putchar('\n'); return nil; }

/* section 12: adding readline with history */
L f_load(L t,L *_) { L x = car(t); if (!in && T(x) == ATOM) in = fopen(A+ord(x),"r"); return x; }

/* section 14: error handling and exceptions */
L f_catch(L t,L *e) {
 I i; L x,**saved[2] = {xb,xp};                                 /* save old xb and xp exception stack pointers */
 jmp_buf savedjb;
 memcpy(savedjb,jb,sizeof(jb));
 if (!xp) xp = xstk;                                            /* set exception stack pointer xp if not set */
 xb = xp;                                                       /* set base stack pointer xb for evals after f_catch */
 if ((i = setjmp(jb)) == 0) x = eval(car(t),*e);
 memcpy(jb,savedjb,sizeof(jb));
 xb = saved[0]; xp = saved[1];                                  /* restore xb and xp exception stack pointers */
 return i == 0 ? x : i == 4 ? err(4,nil) : cons(atom("ERR"),i);
}
L f_throw(L t,L *_) { return err(num(car(t)),nil); }

/* section 13: execution tracing */
L f_trace(L t,L *_) { tr = not(t) ? !tr : (I)num(car(t)); return num(tr); }

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
 {0}};

/* section 13: tracing (trace 1) with colorful output, to wait on ENTER (trace 2), with memory dump (trace 3) */
void trace(L y,L x,L e) {
 if (tr > 2 && !equ(e,env)) { printf("\n\e[35mENV: \e[33m"); print(e); printf("\e[m"); }
 printf("\n\e[32m%u \e[33m",lp); print(y); printf("\e[36m => \e[33m"); print(x); printf("\e[m\t");
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
  g = f; f = eval(car(x),e); x = cdr(x);
  if (T(f) == PRIM) {
   /* apply Lisp primitive to argument list x, return value in x */
   x = prim[ord(f)].f(x,&e);
   /* garbage collect g = old f, garbage collect old macro body h */
   gc(g); g = nil; gc(h); h = nil;
   /* if tail-call then continue evaluating x, otherwise return x */
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) == MACR) {
   /* bind macro f variables v to the given arguments literally (i.e. without evaluating the arguments) */
   for (d = dup(env),v = car(f); T(v) == CONS; v = cdr(v),x = cdr(x)) d = pair(car(v),dup(car(x)),d);
   if (T(v) == ATOM) d = pair(v,x,d);
   /* expand macro f, then continue evaluating the expanded x */
   x = eval(cdr(f),d);
   /* garbage collect bindings d, gabage collect g = old f and old macro body h, save macro body h = x to gc later */
   gc(d); d = nil; gc(g); g = nil; gc(h); h = x;
   continue;
  }
  if (T(f) != CLOS) return err(3,f);
  /* get the list of variables v of closure f */
  v = car(car(f)); d = dup(cdr(f));
  if (T(d) == NIL) d = dup(env);
  /* bind closure f variables v to the evaluated argument values */
  for (a = 0; T(v) == CONS; v = cdr(v)) d = pair(car(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = a ? pair(v,dup(x),d) : evlis(v,x,e,&d);
  /* next, evaluate body x of closure f in environment e = d while keeping f in memory as long as x */
  x = cdr(car(f));
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
 if (seeing('(') || seeing(')') || seeing('\'') || seeing('`') || seeing(',')) buf[i++] = get();
 else do buf[i++] = get(); while (i < 39 && !seeing('(') && !seeing(')') && !seeing(' '));
 return buf[i] = 0,*buf;
}
L Read() { return scan(),parse(); }

/* section 16.1: replacing recursion with loops (in list parsing) */
L list() {
 L t,*p;
 for (t = nil,p = &t; ; *p = cons(parse(),nil),p = &cell[ord(*p)]) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = Read(),scan(),t;
 }
}
L tick() {
 L t,*p;
 if (*buf == ',') return Read();
 if (*buf != '(') return cons(atom("quote"),cons(parse(),nil));
 for (t = cons(atom("list"),nil),p = &cell[ord(t)]; ; *p = cons(tick(),nil),p = &cell[ord(*p)])
  if (scan() == ')') return t;
}
L parse() {
 L n; I i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(Read(),nil));
 if (*buf == '`') return scan(),tick();
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

/* section 14: error handling and exceptions */
void stop(int i) { if (line) err(5,nil); else abort(); }

/* section 10: read-eval-print loop (REPL) with additions */
int main(int argc,char **argv) {
 I i; L x; printf("tinylisp");
 for (i = 2; i < N; i += 2) ref[i] = i-2;
 nil = box(NIL,0); atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 in = fopen((argc > 1 ? argv[1] : "common.lisp"),"r");
 using_history();
 if ((i = setjmp(jb)) > 0) printf("ERR %u",i);
 signal(SIGINT,stop);
 while (1) { rebuild(); putchar('\n'); snprintf(ps,20,"%u>",2*fn-hp/8); print(gc(eval(x = Read(),env))); gc(x); }
}
