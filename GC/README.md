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
  - new primitives `list` and `append` for backquoting without having to load list.lisp (+16 lines of C)
  - upgrades `read` to take an optional pathname to read a Lisp expression from a file (+12 lines of C)
  - upgrades `load` to load multiple files and permit nesting up to 10 levels deep (+14 lines of C)
  - the source code is commented to explain the code
  - passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`
  - optimized internal logic with unchecked CAR and CDR when safe to use
  - compile with `cc -O2 -o tinylisp tinylisp-extras-gc.c -lreadline`

- [tinylisp-extras-expand-gc.c](tinylisp-extras-expand-gc.c)
  - the ultimate version of the above with a lot more built-in extras and automatic hygienic macros
  - fast interpreter optimized with early binding names to globals (part of early macro expansion)
  - also adds a mark-sweep garbage collector that kicks in when a program runs low on memory (deletes unreachable cyclic data structures)

**Tinylisp versions with mark-sweep garbage collector**

- [tinylisp-extras-ms.c](tinylisp-extras-ms.c)
  - based on tinylisp-extras.c that includes all of the article's extras (+184 lines of C)
  - adds mark-sweep garbage collection, supporting three modes `MS=0`, `MS=1`, and `MS=2`
  - new primitive [`atomize`](atomize.lisp) (convert expressions to atom) (+37 lines of C)
  - new primitive `write-to` (redirect print/ln to a file) (+16 lines of C)
  - new primitives `list` and `append` for backquoting without having to load list.lisp (+16 lines of C)
  - upgrades `read` to take an optional pathname to read a Lisp expression from a file (+12 lines of C)
  - upgrades `load` to load multiple files and permit nesting up to 10 levels deep (+14 lines of C)
  - the source code is commented to explain the code
  - passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`
  - optimized internal logic with unchecked CAR and CDR when safe to use
  - compile with `cc -DMS=2 -O2 -o tinylisp tinylisp-extras-ms.c -lreadline` or without `MS=2` for speed, but with possible runtime atom symbol allocation problems due to memory fragmentation

- [tinylisp-extras-expand-ms.c](tinylisp-extras-expand-ms.c)
  - the ultimate version of the above with a lot more built-in extras and automatic hygienic macros
  - fast interpreter optimized with early binding names to globals (part of early macro expansion)

**Reference counting or mark-sweep, which is faster?**

Reference counting continuously releases unused memory (i.e. unused cons cell
pairs that form lists) back into the pool to recycle for reuse.  By contrast,
mark-sweep only collects unused memory to recycle for reuse when the
interpreter runs out of memory.

The tinylisp-extras-gc reference count garbage collector does all the heavy
lifting efficiently and uniformly, independent of the cell memory size.
However, memory size has an impact on tinylisp-extras-ms with mark-sweep
garbage collection.

The tinylisp-extras-expand versions use early binding to boost performance
significantly.  This avoids the runtime overhead of `assoc()` calls for most
globals.  Adding built-ins, in particular `make-list`, `nth`, and `seq`,
further speeds up solving the 8-queens benchmark.

A quick investigation (not scientific) shows the performance difference on a
Mac M1 compiled with clang 21.0.0 option -O2 to solve the
[nqueens.lisp](nqueens.lisp) problem for N=8, we get the following average
compute times of 10 or more runs with `show` and `print` output removed from
`nqueens.lisp`:

| implementation | GC method | mem size (cells) | time (ms) |
| -------------- | --------- | ---------------: | --------: |
| tinylisp-extras-gc                                        | ref count              |  8192 |  373 ms |
| tinylisp-extras-expand-gc                                 | ref count + mark-sweep |  8192 |  105 ms |
| tinylisp-extras-expand-gc (with additional built-ins)     | ref count + mark-sweep |  8192 |   35 ms |
| tinylisp-extras-ms                                        | mark-sweep mode `MS=0` |  8192 |  370 ms |
| tinylisp-extras-expand-ms                                 | mark-sweep mode `MS=0` |  8192 |   85 ms |
| tinylisp-extras-expand-ms (with additional built-ins)     | mark-sweep mode `MS=0` |  8192 |   28 ms |
| tinylisp-extras-ms                                        | mark-sweep mode `MS=0` | 16384 |  365 ms |
| tinylisp-extras-expand-ms                                 | mark-sweep mode `MS=0` | 16384 |   79 ms |
| tinylisp-extras-expand-ms (with additional built-ins)     | mark-sweep mode `MS=0` | 16384 |   27 ms |
| [lisp](https://github.com/Robert-van-Engelen/lisp)        | mark-sweep             |  8192 |  920 ms |
| [lisp](https://github.com/Robert-van-Engelen/lisp)        | mark-sweep             | 16384 |  895 ms |
| [lisp-cheney](https://github.com/Robert-van-Engelen/lisp) | cheney                 |  8192 | 1880 ms |
| [lisp-cheney](https://github.com/Robert-van-Engelen/lisp) | cheney                 | 16384 | 1420 ms |

The performance of reference count GC is independent of the memory size (since
there is no effect, different memory sizes are not shown in the table for
tinylisp with ref count GC).  Memory size does impact mark-sweep and cheney,
where more memory reduces GC overhead.

The performance of tinylisp-extras versus the Common Lisp interpreter GNU
[CLISP](https://www.gnu.org/software/clisp) is reasonably comparable (370 ms
versus CLISP 296 ms) to solve 8-queens.  However, tinylisp-extras, lisp, and
lisp-cheney are all slow due to the runtime overhead of frequent `assoc()`
calls in `eval()` to find the definitions of globals in the environment.

Mark-sweep in tinylisp-extras-expand-gc with reference counting has zero
overhead since there are no unreachable cyclic data structures in the 8-queends
benchmark that ref count cannot delete.

Mark-sweep in tinylisp-extras-expand-ms has three operating modes:

- `MS=0` allocates cell pairs until running out of memory, i.e. the fastest
  method, but this method may cause fragmentation that blocks the allocation of
  new atom symbols (located below in the cell pair pool), causing a fatal
  out-of-memory error
- `MS=1` allocates until 1/2 or 1/4 or 1/8 or ... free cell memory remains to
  avoid fragmentation, but this may cause out-of-control mark-sweep calls when
  repeately crossing the same free cell ratio, e.g. allocate one cell pair that
  triggers mark-sweep only to release one other cell pair, and so on
- `MS=2` is similar to `MS=1` but avoids out-of-control mark-sweep by
  disallowing crossing the same free cell ratio to invoke mark-sweep again,
  that is, after crossing 1/2 to mark-sweep it requires crossing 1/4, then
  either crossing 1/2 or 1/8, and so on.  This operating mode keeps
  fragmentation low with good performance.

The effect of these modes on the performance of tinylisp-extras-expand-ms (with
additional built-ins) for small 2048 to larger 65536 cell memory sizes is
clearly noticible:

| implementation | GC method | mem size (cells) | time (ms) | GC invocations |
| -------------- | --------- | ---------------: | --------: | -------------: |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` |  2048 |   79 ms | 11,771 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` |  4096 |   33 ms |  1,547 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` |  8192 |   28 ms |    569 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` | 16384 |   27 ms |    251 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` | 32768 |   26 ms |    119 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=0` | 65536 |   26 ms |     57 |
|                           |                        |       |         |        |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` |  2048 |  233 ms | 46,354 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` |  4096 |  100 ms | 11,590 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` |  8192 |   38 ms |  1,544 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` | 16384 |   32 ms |    568 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` | 32768 |   30 ms |    251 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=1` | 65536 |   30 ms |    119 |
|                           |                        |       |         |        |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` |  2048 |  150 ms | 26,741 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` |  4096 |   52 ms |  4,362 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` |  8192 |   33 ms |  1,080 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` | 16384 |   30 ms |    432 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` | 32768 |   29 ms |    196 |
| tinylisp-extras-expand-ms | mark-sweep mode `MS=2` | 65536 |   28 ms |     94 |

Note that `MS=1` effectively cuts memory size in half, not surprisingly. It
suffers some out-of-control behavior at 2048 and 4096 memory sizes.

`MS=2` is recommended, where at `N=8192` the performance of 33 ms is slightly
better than the 35 ms that reference count tinylisp-extras-expand-gc takes to
solve 8-queens.  Adding more cell memory can bring 33 ms down to 28 ms.
However, the performance of mark-sweep with different cell memory sizes is
application dependent.  One benchmark does not provide sufficient performance
testing coverage.  The 8-queens benchmark destructively updates lists, without
creating new ones.  By constrast, Lisp programs that frequently construct
temporary lists may benefit more from reference counting that removes them
quickly and continously.

Let's compare this to [SBCL](https://www.sbcl.org) which is a high-performance
Common Lisp implementation that internally compiles Common Lisp programs to
machine code to run.  It runs 8-queens in 6 ms or in 5 ms with safety off and
max optimizations.  However, Common Lisp (compiled or not) is not as flexible
as tinylisp in which code and data are truly the same.  The dot operator is
supported by tinylisp as should be and there is no need for ugly `funcall` and
other unnecessary additions.

Perhaps I will build a compiler for tinylisp.  The fastest way to run tinylisp
programs is to generate C code that is highly optimizable by a C compiler.
Solving 8-queens in compiled tinylisp should take about 2 ms, a best estimate
based on my prototype tinylisp compiler.

**How does reference count GC work?**

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
do this when we return to the REPL by recounting all cell references then
sweeping all zero reference cells into the free memory list:

```c
void count(L x) {
  if ((T(x) == CONS || T(x) == CLOS || T(x) == MACR) && !ref[ord(x)/2]++) {
    count(cell[ord(x)+1]); count(cell[ord(x)]);
  }
}
void sweep() {
  for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2)
    if (!ref[i/2]) del(i); else lomem(i);
}
void rebuild() { memset(ref,0,sizeof(ref)); count(env); sweep(); }
```

where `rebuild()` resets all reference counts to zero, then `count(env)`
recursively traverses all cells reachable from `env` to increase the reference
count by one for each cell pair referenced.  The car `cell[i+1]` and cdr
`cell[i]` are recursively marked when `ref[i/2]++ == 0`, which is when the cell
pair is visited for the very first time.  Subsequent visits only increase the
reference count without recursing to rebuild the reference counts.

After `count()` determines the reference count for each cell pair, `sweep()`
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

In Lisp the `letrec` and `letrec*` construct cyclic local environments for
recursive lambda closures.  Since it is known when and where this happens, I am
using strongly connected component (SCC) analysis to identify these structures
to delete them later as part of the reference count garbage collection strategy
implemented in tinylisp-extras-gc and tinylisp-extras-expand-gc.

Local recursive lambdas in `letrec` and `letrec*` are detected in *O(1)* time
each, by checking if the reference count of the local lambda increases.
Then, when detected, constructing an SCC takes *O(n)* time for the *n* cells
that constitute the cycle and cells referenced from the cycle in the local
environment of the `letrec` and `letrec*`.  Typically *n* is small, only a few
cells make up the local environment in the SCC.  SCC construction is done only
once when evaluating the `letrec` or `letrec*`, even when the local functions
recurses many times.  Garbage collecting the SCC afterwards only takes *O(1)*
unit time to check if all references to the SCC are gone, then it takes *O(n)*
time to delete the entire SCC.

Furthermore, since tinylisp also implements `catch` and `throw`, I've added a
stack that is used whenever `catch` is called.  This stack "remembers" all C
variables used in the interpeter that may point to lists that must be collected
when `throw` returns control to the `catch`.

The tinylisp-extras-expand-gc version also implements early binding of globals
for speed.  This version also has automatic hygienic macros and adds many more
extra built-ins (primitives).  An extra mark-sweep garbage collector is added
that kicks in when a Lisp program runs low on memory to delete cyclic data
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
Calling this function with `(loopy)` does not run out of memory in
tinylisp-extras-expand-gc.  In fact, efficient tail-call recursion combined with
reference counting and mark-sweep garbage collection makes this call never
terminate.
