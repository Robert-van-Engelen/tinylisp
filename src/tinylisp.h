#ifndef __TINY_LISP_H
#define __TINY_LISP_H

/* we only need two types to implement a Lisp interpreter:
        I    unsigned integer (either 16 bit, 32 bit or 64 bit unsigned)
        L    Lisp expression (double with NaN boxing)
   I variables and function parameters are named as follows:
        i    any unsigned integer, e.g. a NaN-boxed ordinal value
        t    a NaN-boxing tag
   L variables and function parameters are named as follows:
        x,y  any Lisp expression
        n    number
        t    list
        f    function or Lisp primitive
        p    pair, a cons of two Lisp expressions
        e,d  environment, a list of pairs, e.g. created with (define v x)
        v    the name of a variable (an atom) or a list of variables
*/
#define I unsigned
#define L double

/* address of the atom heap is at the bottom of the cell stack */
#define A ((char *)cell)

/* number of cells for the shared stack and atom heap, increase N as desired */
#define N (1024)
// Shared stack and atom heap.
extern L cell[N];

// environment and stack related things
extern L env;
extern I hp, sp;

extern void gc(void);

// Scanner things.
extern char scan(void);
#define MAX_SCAN_BUF 40
extern char buf[MAX_SCAN_BUF];

// Parsing and evaluation
extern L eval(L exp, L env);
extern L Read(void);

// These are needed to deconstruct NaNs.
extern const I ATOM, PRIM, CONS, CLOS, NIL;
/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) (*(unsigned long long *)&x >> 48)
extern I ord(L x);

// Global initialization
extern void init_tinylisp(void);
extern int _main(int argc, char **argv);

#endif  // ifndef __TINY_LISP_H