**Tinylisp versions with reference count garbage collection**

- [tinylisp-gc.c](tinylisp-gc.c)
  - the smallest simplest tinylisp version with gc, based on tinylisp.c (+15 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - performs a mark-sweep cleanup when returning to the REPL
  - extra: error handling to return to the REPL with an error message
  - extra: reopens on EOF so `cat common.lisp list.lisp | ./tinylisp` parses files before REPL
  - passes `tests/dotcall.lisp` tests
  - compile with `cc -O2 -o tinylisp tinylisp-gc.c`

- [tinylisp-opt-gc.c](tinylisp-opt-gc.c)
  - a simple tinylisp version with gc, based on tinylisp-opt.c (+11 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - performs a mark-sweep cleanup when returning to the REPL
  - extra: error handling to return to the REPL with an error message
  - extra: reopens on EOF so `cat common.lisp list.lisp | ./tinylisp` parses files before REPL
  - passes `tests/dotcall.lisp` tests
  - compile with `cc -O2 -o tinylisp tinylisp-opt-gc.c`

- [tinylisp-extras-gc.c](tinylisp-extras-gc.c)
  - based on tinylisp-extras.c that includes all of the article's extras (+180 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - garbage collects cycles in `letrec` and `letrec*` recursive local functions using strongly connected component analysis
  - cleans up `catch`-`throw` exceptions in Lisp using a temporary stack when `catch` is used
  - performs a mark-sweep cleanup when returning to the REPL
  - includes a memory debugger, compile with `-DDEBUG` or `-DDEBUG=2` (verbose) to enable
  - the source code is commented to explain the code
  - passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`
  - compile with `cc -O2 -o tinylisp tinylisp-extras-gc.c -lreadline`

See also [#20](https://github.com/Robert-van-Engelen/tinylisp/issues/20)

**How does it work?**

The original tinylisp uses a stack to allocate new cells for `CONS` and `CLOS`
Lisp values.  Two cells are needed to store the car and the cdr of a `CONS` or
`CLOS` value.  The ordinal of a `CONS` and `CLOS` NaN-boxed value is the index
of the cell pair on the stack.  In this way, allocation is fast and simple.
But deallocation and reuse is not possible until we return to the REPL to throw
everytying away that was computed, except for the global environemnt `env`.

To collect cells that are no longer used to reuse them later, i.e. to *garbage
collect* them, we need a pool of cells instead of a stack.  With a pool of free
cell pairs we can allocate cells for `CONS` and `CLOS` values from the pool and
return them to the pool when we are done with them.

All available free cell pairs in the pool are managed in a single *free list*
that links together all free cell pairs, linking them via a `ref[]` array.  The
head of the free list of cell pairs is indexed by the *free cell pairs list
pointer* `fp`.  Therefore, `cell[fp]` and `cell[fp+1]` is the first free cell
pair in this list.  The next free cell pair index is `ref[fp/2]`.  Note that we
only need half the number of `ref[]` entries compared to `cell[]`, so `ref[i/2]`
corresponds to `cell[i]` and `cell[i+1]`.

For a pool of `cell[N]` cells we allocate `ref[N/2]` indices:

```c
I ref[N/2],hp,fp,lp,fn
```

Hence, we remove the stack pointer `sp` from tinylisp, but add a new `fp` free
cell pairs list pointer, a pointer `lp` to the lowest allocated and used cell
pair to detect memory overflows, and `fn` the number of free cell pairs.  Note
that when we say "pointer" we mean an index into the `cell[]` array that it
points to.

The lowest allocated and used cell pair pointer `lp` is updated in a new
`lomem()` function:

```c
I lomem(I i) { return lp = i < lp ? i : lp; }
```

A new cell pair `cell[i]` and `cell[i+1]` is allocated and returned as a
NaN-boxed `box(CONS,i)` with a new `alloc()` function.  It also checks if
`lomem(i)` does not overflow into the atom heap:

```c
L alloc() { I i = fp; fp = ref[i/2]; ref[i/2] = 1; --fn; return hp > lomem(i)<<3 ? err(4) : box(CONS,i); }
```

Because `ref[i/2]` becomes unused when the corresponding cell pair is
allocated, we will reuse `ref[i/2]` to store the reference count of the cell
pair, which is initially one, i.e. `ref[i/2] = 1`.

The following updated tinylisp `cons()` function calls `alloc()` and stores the
car `x` and cdr `y` of the new pair `p`:

```c
L cons(L x,L y) { L p = alloc(); cell[ord(p)+1] = x; cell[ord(p)] = y; return p; }
```

With reference count garbage collection we need to "duplicate" a Lisp
expression whenever we want to use it without risking it from being garbage
collected in some other part of the Lisp interpreter.  This "duplication" only
applies to `CONS` and `CLOS` values that point to cell pairs.  To duplicate, we
simply increase the reference count `ref[i/2]` by one when the expression to
duplicate is a `CONS` or `CLOS` value that uses `cell[ord(x)+1]` for its car
and `cell[ord(x)]` for its cdr:

```c
L dup(L x) { if ((T(x)&~(CONS^CLOS)) == CONS) ++ref[ord(x)/2]; return x; }
```

Every `dup(x)` must be followed sooner or later by a `gc(x)` to collect it when
`x` is a pair whose reference count drops to zero:

```c
void del(I i) { ref[i/2] = fp; fp = i; ++fn; }
void gc(L x) { I i; if ((T(x)&~(CONS^CLOS)) == CONS && !--ref[(i = ord(x))/2]) { del(i); gc(cell[i+1]); gc(cell[i]); } }
```

This decrements the reference count `ref[i/2]` of a `CONS` or `CLOS` value `x`
with cell pair index `i = ord(x)` and calls `del(i)` to reclaim the cell pair
car `cell[i+1]` and cdr `cell[i]` by adding it to the head of the free list
pointed to by `fp`.  The number of free cell pairs `fn` is increased by one.
This is recursively repeated for the car `cell[i+1]` and cdr `cell[i]` to
collect them.

Note that cyclic data structures formed by lists cannot be garbage collected
with reference counting, because there is at least one cell pair that is
referenced by a back-edge from the data and this cell pair's reference count
never drops to zero.  Cyclic data structures cannot be created in tinylisp
though, as long as we don't extend the implementation with `letrec` and
`letrec*` local recursive lambda closures and with `setq`, `set-car!` and
`set-cdr!` that allow destructive assignments with which cyclic data structures
can be created.

Cyclic data structures can be collected with mark-sweep garbage collection.  We
do this when we return to the REPL:

```c
void mark(L x) { if ((T(x)&~(CONS^CLOS)) == CONS && !ref[ord(x)/2]++) { mark(cell[ord(x)+1]); mark(cell[ord(x)]); } }
void sweep() { for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2) if (!ref[i/2]) del(i); else lomem(i); }
void rebuild() { memset(ref,0,sizeof(ref)); mark(env); sweep(); }
```

where `rebuild()` resets all reference counts to zero, then `mark(env)`
recursively traverses all cells reachable from `env` marking them used with a
nonzero reference count.  The car `cell[i+1]` and cdr `cell[i]` are recursively
marked when `ref[i/2]++ == 0`, which is when the cell pair is visited for the
very first time.  Subsequent visits only increase the reference count without
recursing.

After `mark()` determines the reference count for each cell pair, `sweep()`
deletes all unused cell pairs by reclaiming them with `del(i)` and also
determines the `lomem(i)` pointer `lp` over all used cell pairs.

Rebuilding the cell memory has another advantage by effectively "linearizing"
the free cell pairs list to run from the top-most free cell to the bottom-most
free cell in the pool.  This helps to avoid allocating bottom cells first that
may run into the atom heap below when the atom heap grows.  This would block
new `define` global named definitions even when we have plenty of free cells
available in the middle of the pool.

If we also want to delete unused atoms from the atom heap, then we can do this
effectively by adding the following two lines to `sweep()` similar to the
article's recommendation:

```c
void sweep() {
 I i; for (hp = 0,i = 0; i < N; ++i) if (ref[i/2] && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
 for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2) if (ref[i/2]) lomem(i); else del(i);
}
```

The main program initializes the free cell pairs pool with `env = 0; rebuild()`.
Then performs garbage collection in the REPL on the `Read()` Lisp expression
`x` parsed from the input and the evaluated value `y` produced:

```c
int main() {
 ...
 env = 0; rebuild();
 ...
 while (1) { L x,y; rebuild(); printf("\n%u>",2*fn-hp/8); print(y = eval(x = Read(),env)); gc(y); gc(x); }
```

Besides these changes to the tinylisp interpreter, also the Lisp primitives and
the tinylisp interpreter logic must be updated throughout the code to call
`dup()` and `gc()` at the necessary points.  Furthermore, the `eval()`,
`evlis()` and `evarg()` functions return a new value including new lists, which
doesn't need a `dup()`, but the returned lists must eventually be `gc()`
collected.

**Garbage collection in the tinylisp "extras" version**

The tinylisp-extras-gc version implements features that may construct cyclic
data structures from lists.  In particular `letrec` and `letrec*` construct
cyclic local environments for recursive lambda closures.  Since it is known
when and where this happens, I am using strongly connected component (SCC)
analysis to identify these structures to delete them later as part of the
reference count garbage collection strategy implemented in this "extras"
version.

Constructing an SCC, when it is detected in a `letrec` or `letrec*` for
recursive functions, takes *O(n)* time for *n* cells that constitute the source
code lists of the recursive local `letrec` and `letrec*` function definitions.
This is done only once when evaluating the `letrec` or `letrec*`, even when the
local functions recurses many times.  Garbage collecting the SCC afterwards
only takes *O(1)* unit time to check if all references to the SCC are gone,
then it takes *O(n)* time to delete the entire SCC.

Furthermore, since this version also implements `catch` and `throw`, I've added
an exception stack that is used whenever `catch` is called.  This exception
stack "remembers" all C variables used in the interpeter that may point to
lists that must be collected when `throw` returns control to the `catch`.

