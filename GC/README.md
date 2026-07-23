**Tinylisp versions with reference counting garbage collector**

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
  - based on tinylisp-extras.c that includes all of the article's extras (+184 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - collects unused cyclic lists created by `letrec` and `letrec*` recursive local functions
  - cleans up `catch`-`throw` exceptions in Lisp using a temporary stack when `catch` is used
  - performs a mark-sweep cleanup when returning to the REPL
  - includes a memory debugger, compile with `-DDEBUG` or `-DDEBUG=2` (verbose) to enable
  - new primitive [`atomize`](atomize.lisp) (convert expressions to atom) (+37 lines of C)
  - new primitive `write-to` (redirect print/ln to a file) (+16 lines of C)
  - new primitive `type` to type check, 0=number, 1=atom, 2=primitive, 3=pair, 4=closure, 5=macro, 6=nil (+4 lines of C)
  - new primitives `list` and `append` for backquoting without having to load list.lisp (+16 lines of C)
  - upgrades `read` to take an optional pathname to read a Lisp expression from a file (+12 lines of C)
  - upgrades `load` to load multiple files and permit nesting up to 10 levels deep (+14 lines of C)
  - the source code is commented to explain the code
  - passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`
  - optimized internal logic with unchecked CAR and CDR when safe to use
  - compile with `cc -O2 -o tinylisp tinylisp-extras-gc.c -lreadline`

- [tinylisp-extras-expand-gc.c](tinylisp-extras-expand-gc.c)
  - the ultimate version of the above with a lot more built-in extras and automatic hygienic macros
  - also adds a mark-sweep garbage collector that kicks in when a program runs low on memory (deletes unreachable cyclic data structures)

See also [#20](https://github.com/Robert-van-Engelen/tinylisp/issues/20)

**Reference counting or mark-sweep, which is faster?**

Reference counting continuously releases unused memory (i.e. unused cons cell
pairs that form lists) back into the pool to recycle for reuse.  By contrast,
mark-sweep only collects unused memory to recycle for reuse when the
interpreter runs out of memory.  Mark-sweep may seem simple and fast, however,
integrating a full mark-sweep impacts memory management overall, because it
also requires an auxiliary stack to keep track of all temporary lists that are
being constructed by the interpreter that cannot be released yet.

The tinylisp reference count garbage collector does all the heavy lifting more
efficiently, while its simple mark-sweep collector only runs when the tinylisp
interpreter returns to the REPL.  In this case, mark-sweep only keeps list
cells that make up the stored Lisp program.  Therefore, we don't need an
auxiliary stack.

A quick investigation (not scientific) shows the performance difference on a
Mac M1 compiled with clang 21.0.0 option -O2 to solve the
[nqueens.lisp](nqueens.lisp) problem for N=8:

| implementation | GC | mem size (cells) | time (ms) |
| -------------- | -- | ---------------: | --------: |
| tinylisp-extras-gc                                        | ref count              |  8192 |  396 ms |
| tinylisp-extras-expand-gc                                 | ref count + mark-sweep |  8192 |   51 ms |
| [lisp](https://github.com/Robert-van-Engelen/lisp)        | mark-sweep             |  8192 |  920 ms |
| [lisp](https://github.com/Robert-van-Engelen/lisp)        | mark-sweep             | 16384 |  895 ms |
| [lisp-cheney](https://github.com/Robert-van-Engelen/lisp) | cheney                 |  8192 | 1880 ms |
| [lisp-cheney](https://github.com/Robert-van-Engelen/lisp) | cheney                 | 16384 | 1420 ms |

Tinylisp is a clear winner!  It is faster and the memory size has no effect on
the running time of tinylisp (since there is no effect, different memory sizes
are not shown in the table for tinylisp).  But memory size does impact
mark-sweep and cheney, since more memory means lower GC overhead.

The performance of tinylisp-extras-gc versus the Common Lisp interpreter GNU
[CLISP](https://www.gnu.org/software/clisp) is reasonably comparable (396 ms
versus CLISP 296 ms) to solve 8-queens.

The performance of tinylisp-extras-expand-gc is significantly boosted with
early binding global names and built-ins.  Mark-sweep in
tinylisp-extras-expand-gc is only used to delete unreachable cyclic data
structures that ref count cannot delete.

By comparison, [SBCL](https://www.sbcl.org) is a high-performance Common Lisp
implementation.  It runs 8-queens in 6 ms.  However, Common Lisp (compiled or
not) is not as flexible as tinylisp in which code and data are truly the same.
The dot operator is supported by tinylisp as should be and there is no need for
ugly `funcall` and other unnecessary additions.

Perhaps I will build a compiler for tinylisp.  The fastest way to run tinylisp
programs is to generate C code that is highly optimizable by a C compiler.
Solving 8-queens in compiled tinylisp should take about 2 ms, a best estimate
based on my prototype tinylisp compiler.

**How does it work?**

The original tinylisp uses a stack to allocate new cells for `CONS` and `CLOS`
Lisp values.  Two cells are needed to store the car and the cdr of a `CONS` or
`CLOS` value.  The ordinal of a `CONS` and `CLOS` NaN-boxed value is the index
of the cell pair on the stack.  In this way, allocation is fast and simple.
But deallocation and reuse is not possible until we return to the REPL to throw
everytying away that was computed, except for the global environment `env`.

To collect cells that are no longer used to reuse them later, i.e. to *garbage
collect* them, we need a pool of cells instead of a stack.  With a pool of free
cons cell pairs we can allocate cells for `CONS` and `CLOS` values from the
pool and return them to the pool when we are done with them.

All available free cons cell pairs in the pool are managed in a single *free
list* that links together all free cons cell pairs, linking them via a `ref[]`
array.  The head of the free list of cons cell pairs is indexed by the *free
cons cell pairs list pointer* `fp`.  Therefore, `cell[fp]` and `cell[fp+1]` is
the first free cell pair in this list.  The next free cell pair index is
`ref[fp/2]`.  Note that we only need half the number of `ref[]` entries
compared to `cell[]`, so `ref[i/2]` corresponds to `cell[i]` and `cell[i+1]`.

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
I alloc() { I i = fp; fp = ref[i/2]; ref[i/2] = 1; --fn; return hp > lomem(i)<<3 ? (I)err(4,nil) : i; }
```

Because `ref[i/2]` becomes unused when the corresponding cell pair is
allocated, we will reuse `ref[i/2]` to store the reference count of the cell
pair, which is initially one, i.e. `ref[i/2] = 1`.

The following updated tinylisp `cons()` function calls `alloc()` and stores the
car `x` and cdr `y` of the new pair `p`:

```c
L cons(L x,L y) { I i = alloc(); cell[i+1] = x; cell[i] = y; return box(CONS,i); }
```

With reference count garbage collection we need to "duplicate" a Lisp
expression whenever we want to use it without risking it from being garbage
collected in some other part of the Lisp interpreter.  This "duplication" only
applies to `CONS` and `CLOS` values that point to cell pairs.  To duplicate, we
simply increase the reference count `ref[i/2]` by one when the expression to
duplicate is a `CONS` or `CLOS` value that uses `cell[ord(x)+1]` for its car
and `cell[ord(x)]` for its cdr:

```c
L dup(L x) { if (T(x) == CONS || T(x) == CLOS || T(x) == MACR) ++ref[ord(x)/2]; return x; }
```

Every `dup(x)` must be followed sooner or later by a `gc(x)` to collect it when
`x` is a pair whose reference count drops to zero:

```c
void del(I i) { ref[i/2] = fp; fp = i; ++fn; }
void gc(L x) {
  I i;
  if ((T(x) == CONS || T(x) == CLOS || T(x) == MACR) && !--ref[(i = ord(x))/2]) {
    del(i); gc(cell[i+1]); gc(cell[i]);
  }
}
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
void mark(L x) {
  if ((T(x) == CONS || T(x) == CLOS || T(x) == MACR) && !ref[ord(x)/2]++) {
    mark(cell[ord(x)+1]); mark(cell[ord(x)]);
  }
}
void sweep() {
  for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2)
    if (!ref[i/2]) del(i); else lomem(i);
}
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
 I i;
 for (hp = 0,i = 0; i < N; ++i)
   if (ref[i/2] && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
 for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2)
   if (ref[i/2]) lomem(i); else del(i);
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

The tinylisp-extras-gc version implements `letrec` and `letrec*` that construct
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

The tinylisp-extras-expand-gc version also implements early binding of globals
for speed and has automatic hygienic macros.  It also adds many more extra
built-ins (primitives).  An extra mark-sweep garbage collector is added that
kicks in when a Lisp program runs low on memory to delete cyclic data
structures.  This extra mark-sweep garbage collector is useful only when Lisp
programs construct cyclic data structures.  Reference-count garbage collection
does all the heavy lifting efficiently.  An example function that contructs
cyclic data structures with `set-cdr!` while recursively calling itself:

```lisp
(define loopy
    (lambda ()
        (progn
            (let* (t (list 1 2 3))
                (set-cdr! (cdr (cdr t)) t))
            (loopy))))
```
Calling this function `(loopy)` does not run out of memory in
tinylisp-extras-expand-gc.  In fact, efficient tail-call recursion combined with
reference counting and mark-sweep garbage collection makes this call never
terminate.

