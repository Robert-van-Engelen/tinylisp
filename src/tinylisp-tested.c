/* tinylisp-commented.c with NaN boxing by Robert A. van Engelen 2022 */
/* tinylisp.c but adorned with comments in an (overly) verbose C style */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinylisp.h"

/* hp: heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   sp: stack pointer, the stack starts at the top of cell[] with sp=N
   safety invariant: hp <= sp<<3 */
I hp = 0, sp = N;

/* atom, primitive, cons, closure and nil tags for NaN boxing
  Note that:
  MSB 1 bit is for sign - set to 0 as the 7 has that bit set to 0.
  the following 11 bits set at 1 indicate a NaN number, these are the three low
  bits that make up the first 7 and subsequent FF. Next bit set at 1 indicates a
  silent NaN The next 3 bits are the ones that tags use - so there are max 8
  values, but we need 5.
 */
const I ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb, NIL = 0x7ffc;

/* cell[N] array of Lisp expressions, shared by the stack and atom heap */
L cell[N];

/* Lisp constant expressions () (nil), #t, ERR, and the global environment env
 */
L nil, tru, err, env;

/* NaN-boxing specific functions:
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
L box(I t, I i) {
  L x;
  *(unsigned long long *)&x = (unsigned long long)t << 48 | i;
  return x;
}

I ord(L x) {
  return *(unsigned long long *)&x; /* the return value is narrowed to 32 bit
                                       unsigned integer to remove the tag */
}

// Clears the tag bits when converting to double or float the NaN value passed
// in.
L num(L n) { return n; }
// bitwise comparison because == does not work on NaN numbers.
I equ(L x, L y) { return *(unsigned long long *)&x == *(unsigned long long *)&y; }

/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM
   Note: Uses heap space thus starting from the bottom of cell array.
   the cell array.
*/
L atom(const char *s) {
  I i = 0;
  while (i < hp && strcmp(A + i, s)) /* search for a matching atom name on the heap */
    i += strlen(A + i) + 1;
  if (i == hp) {                        /* if not found */
    hp += strlen(strcpy(A + i, s)) + 1; /*   allocate and add a new atom name to the heap */
    if (hp > sp << 3)                   /* abort when out of memory */
      abort();
  }
  return box(ATOM, i);
}

/* construct pair (x . y) returns a NaN-boxed CONS
   Note: Uses stack space thus starting from the top of cell array.
*/
L cons(L x, L y) {
  cell[--sp] = x;   /* push the car value x */
  cell[--sp] = y;   /* push the cdr value y */
  if (hp > sp << 3) /* abort when out of memory */
    abort();
  return box(CONS, sp);
}

/* return the car of a pair or ERR if not a pair */
L car(L p) { return (T(p) & ~(CONS ^ CLOS)) == CONS ? cell[ord(p) + 1] : err; }

/* return the cdr of a pair or ERR if not a pair */
L cdr(L p) { return (T(p) & ~(CONS ^ CLOS)) == CONS ? cell[ord(p)] : err; }

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v, L x, L e) { return cons(cons(v, x), e); }

/* construct a closure, returns a NaN-boxed CLOS */
L closure(L v, L x, L e) { return box(CLOS, ord(pair(v, x, equ(e, env) ? nil : e))); }

/* look up a symbol in an environment, return its value or ERR if not found */
L assoc(L v, L e) {
  while (T(e) == CONS && !equ(v, car(car(e)))) e = cdr(e);
  return T(e) == CONS ? cdr(car(e)) : err;
}

/* not(x) is nonzero if x is the Lisp () empty list */
I not(L x) { return T(x) == NIL; }

/* let(x) is nonzero if x is a Lisp let/let* pair */
I let(L x) { return T(x) != NIL && !not(cdr(x)); }

/* return a new list of evaluated Lisp expressions t in environment e */
L eval(L, L);
L evlis(L t, L e) {
  return T(t) == CONS ? cons(eval(car(t), e), evlis(cdr(t), e)) : T(t) == ATOM ? assoc(t, e) : nil;
}

/* Lisp primitives:
   (eval x)            return evaluated x (such as when x was quoted)
   (quote x)           special form, returns x unevaluated "as is"
   (cons x y)          construct pair (x . y)
   (car p)             car of pair p
   (cdr p)             cdr of pair p
   (add n1 n2 ... nk)  sum of n1 to nk
   (sub n1 n2 ... nk)  n1 minus sum of n2 to nk
   (mul n1 n2 ... nk)  product of n1 to nk
   (div n1 n2 ... nk)  n1 divided by the product of n2 to nk
   (int n)             integer part of n
   (< n1 n2)           #t if n1<n2, otherwise ()
   (eq? x y)           #t if x equals y, otherwise ()
   (not x)             #t if x is (), otherwise ()
   (or x1 x2 ... xk)   first x that is not (), otherwise ()
   (and x1 x2 ... xk)  last x if all x are not (), otherwise ()
   (cond (x1 y1)
         (x2 y2)
         ...
         (xk yk))      the first yi for which xi evaluates to non-()
   (if x y z)          if x is non-() then y else z
   (let* (v1 x1)
         (v2 x2)
         ...
         y)            sequentially binds each variable v1 to xi to evaluate y
   (lambda v x)        construct a closure
   (define v x)        define a named value globally */
L f_eval(L t, L e) { return eval(car(evlis(t, e)), e); }

L f_quote(L t, L _) { return car(t); }

L f_cons(L t, L e) {
  t = evlis(t, e);
  return cons(car(t), car(cdr(t)));
}

L f_car(L t, L e) { return car(car(evlis(t, e))); }

L f_cdr(L t, L e) { return cdr(car(evlis(t, e))); }

L f_add(L t, L e) {
  L n;
  t = evlis(t, e);
  n = car(t);
  while (!not(t = cdr(t))) n += car(t);
  return num(n);
}

L f_sub(L t, L e) {
  L n;
  t = evlis(t, e);
  n = car(t);
  while (!not(t = cdr(t))) n -= car(t);
  return num(n);
}

L f_mul(L t, L e) {
  L n;
  t = evlis(t, e);
  n = car(t);
  while (!not(t = cdr(t))) n *= car(t);
  return num(n);
}

L f_div(L t, L e) {
  L n;
  t = evlis(t, e);
  n = car(t);
  while (!not(t = cdr(t))) n /= car(t);
  return num(n);
}

L f_int(L t, L e) {
  L n = car(evlis(t, e));
  return n < 1e16 && n > -1e16 ? (long long)n : n;
}

L f_lt(L t, L e) { return t = evlis(t, e), car(t) - car(cdr(t)) < 0 ? tru : nil; }

L f_eq(L t, L e) { return t = evlis(t, e), equ(car(t), car(cdr(t))) ? tru : nil; }

L f_not(L t, L e) { return not(car(evlis(t, e))) ? tru : nil; }

L f_or(L t, L e) {
  L x = nil;
  while (T(t) != NIL && not(x = eval(car(t), e))) t = cdr(t);
  return x;
}

L f_and(L t, L e) {
  L x = nil;
  while (T(t) != NIL && !not(x = eval(car(t), e))) t = cdr(t);
  return x;
}

L f_cond(L t, L e) {
  while (T(t) != NIL && not(eval(car(car(t)), e))) t = cdr(t);
  return eval(car(cdr(car(t))), e);
}

L f_if(L t, L e) { return eval(car(cdr(not(eval(car(t), e)) ? cdr(t) : t)), e); }

L f_leta(L t, L e) {
  for (; let(t); t = cdr(t)) e = pair(car(car(t)), eval(car(cdr(car(t))), e), e);
  return eval(car(t), e);
}

L f_lambda(L t, L e) { return closure(car(t), car(cdr(t)), e); }

L f_define(L t, L e) {
  env = pair(car(t), eval(car(cdr(t)), e), env);
  return car(t);
}

/* table of Lisp primitives, each has a name s and function pointer f */
struct {
  const char *s;
  L (*f)(L, L);
} prim[] = {
    {"eval", f_eval},
    {"quote", f_quote},
    {"cons", f_cons},
    {"car", f_car},
    {"cdr", f_cdr},
    {"+", f_add},
    {"-", f_sub},
    {"*", f_mul},
    {"/", f_div},
    {"int", f_int},
    {"<", f_lt},
    {"eq?", f_eq},
    {"or", f_or},
    {"and", f_and},
    {"not", f_not},
    {"cond", f_cond},
    {"if", f_if},
    {"let*", f_leta},
    {"lambda", f_lambda},
    {"define", f_define},
    {0}
};

/* create environment by extending e with variables v bound to values t */
L bind(L v, L t, L e) {
  return T(v) == NIL    ? e
         : T(v) == CONS ? bind(cdr(v), cdr(t), pair(car(v), car(t), e))
                        : pair(v, t, e);
}

/* apply closure f to arguments t in environemt e */
L reduce(L f, L t, L e) {
  return eval(cdr(car(f)), bind(car(car(f)), evlis(t, e), not(cdr(f)) ? env : cdr(f)));
}

/* apply closure or primitive f to arguments t in environment e, or return ERR
 */
L apply(L f, L t, L e) {
  return T(f) == PRIM ? prim[ord(f)].f(t, e) : T(f) == CLOS ? reduce(f, t, e) : err;
}

/* evaluate x and return its value in environment e
If the x(pression) passed in is an atom, it's irreducibel so just obtain the value.
If the x(pression) passed in is a list, then delegate to apply which will receive
the (sub-)eval of the first element of the list and as arguments the CDR of the list.
 */
L eval(L x, L e) {
  return T(x) == ATOM ? assoc(x, e) : T(x) == CONS ? apply(eval(car(x), e), cdr(x), e) : x;
}

/* tokenization buffer and the next character that we are looking at */
char buf[MAX_SCAN_BUF], see = ' ';
#define IS_EOF() (see == EOF)

/* advance to the next character */
static inline void look(void) { see = getc(stdin); }

/* return nonzero if we are looking at character c, ' ' means any white space
   as all other white space like \n \r or \t happen before
 */
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }

/* return the look ahead character from standard input, advance to the next */
char get() {
  char c = see;
  look();
  return c;
}

/* tokenize into buf[], return first character of buf[]
The following tokens are recognized:
  '(',')', '\'', ' ', '\t', '\n'
  any string composed of characters not of the above.
*/
char scan() {
  I i = 0;
  // skip whitespace
  while (seeing(' ')) look();
  // If I see one of this, I return only that
  if (seeing('(') || seeing(')') || seeing('\'')) buf[i++] = get();
  else do {
      // else, I collect everything until I get to a closing parentheses or space.
      buf[i++] = get();
    } while (i < (MAX_SCAN_BUF - 1) && !(IS_EOF() || seeing('(') || seeing(')') || seeing(' ')));
  buf[i] = 0;
  return *buf;
}

/* return the Lisp expression read from standard input */
L parse(void);
L Read(void) {
  scan();
  // if (IS_EOF()) return nil;
  return parse();
}

/* return a parsed Lisp list */
L list() {
  L x;
  // First we look for the empty list.
  if (scan() == ')') return nil;
  // Then we look for other lists, which do not
  // use pair syntax.
  if (!strcmp(buf, ".")) {
    x = Read();
    scan();
    return x;
  }
  // Now, we deal with pairs.
  x = parse();
  return cons(x, list());
}

/* return a parsed Lisp expression x quoted as (quote x) */
L quote() { return cons(atom("quote"), cons(Read(), nil)); }

/* return a parsed atomic Lisp expression (a number or an atom) */
L atomic() {
  L n;
  I i;
  return (sscanf(buf, "%lg%n", &n, &i) > 0 && !buf[i]) ? n : atom(buf);
}

// clang-format off
/* return a parsed Lisp expression */
L parse(void) { return *buf == '(' ? list() : 
                       *buf == '\'' ? quote() : 
                       atomic(); 
}
// clang-format on

/* display a Lisp list t */
void print(L);
void printlist(L t) {
  for (putchar('(');; putchar(' ')) {
    print(car(t));
    t = cdr(t);
    if (T(t) == NIL) break;
    if (T(t) != CONS) {
      printf(" . ");
      print(t);
      break;
    }
  }
  putchar(')');
}

/* display a Lisp expression x */
void print(L x) {
  if (T(x) == NIL) printf("()");
  else if (T(x) == ATOM) printf("%s", A + ord(x));
  else if (T(x) == PRIM) printf("<%s>", prim[ord(x)].s);
  else if (T(x) == CONS) printlist(x);
  else if (T(x) == CLOS) printf("{%u}", ord(x));
  else printf("%.10lg", x);
}

/* garbage collection removes temporary cells, keeps global environment */
void gc() { sp = ord(env); }

/* Lisp initialization and REPL */
void init_tinylisp() {
  // reset heap an stack pointers.
  hp = 0, sp = N;
  // initialize scanner data.
  see = ' ';
  // Initialize a few things globally available.
  nil = box(NIL, 0);
  err = atom("ERR");
  tru = atom("#t");
  // environment starts empty.
  env = pair(tru, tru, nil);
  // Initialize primitives, extending environment for each one.
  for (int i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s), box(PRIM, i), env);
}

/*
The read-eval-print-loop

The interactive prompt gives you a so-called read-eval-print-loop (REPL). 
Tinylisp uses the read function to read in a lisp object, 
evaluates it using eval, and prints it using print. Then prompt again. 

The functions eval and apply interact in a fundamental way in the interpreter: 
If we are evaluating an ordinary function call (which is represented as a list, of course),
we evaluate the first element of the list to get the functions,
evaluate the remaining elements to get the arguments, 
and then use apply to apply the function to the arguments.

(Notice that this is using call by value semantics, not lazy evaluation as in Haskell -- 
  the arguments get evaluated before applying the function to them.) 
*/
int _main(int argc, char **argv) {
  printf("tinylisp");
  init_tinylisp();
  while (true) {
    printf("\n%u>", sp - hp / 8);
    L read_data = Read();
    if (IS_EOF()) break;
    print(eval(read_data, env));
    gc();
  }
  return 0;
}
