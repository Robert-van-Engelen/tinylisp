**Tinylisp versions with reference count garbage collection**

- [tinylisp-gc.c](tinylisp-gc.c)
  - the smallest simplest tinylisp version with gc, based on tinylisp.c (+15 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - performs a mark-sweep cleanup when returning to the REPL
  - adds error handling to return to the REPL with an error message
  - passes `tests/dotcall.lisp` tests
  - compile with `cc -O2 -o tinylisp tinylisp-gc.c`

- [tinylisp-opt-gc.c](tinylisp-opt-gc.c)
  - a simple tinylisp version with gc, based on tinylisp-opt.c (+11 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - performs a mark-sweep cleanup when returning to the REPL
  - adds error handling to return to the REPL with an error message
  - passes `tests/dotcall.lisp` tests
  - compile with `cc -O2 -o tinylisp tinylisp-opt-gc.c`

- [tinylisp-extras-gc.c](tinylisp-extras-gc.c)
  - based on tinylisp-extras.c that includes all of the article's extras (+220 lines of C)
  - adds reference count garbage collection to continuously release unused memory cells
  - garbage collects cycles in `letrec` and `letrec*` recursive local functions using strongly connected component analysis
  - cleans up `catch`-`throw` exceptions in Lisp using a temporary stack when `catch` is used
  - performs a mark-sweep cleanup when returning to the REPL
  - includes a memory debugger, compile with `-DDEBUG` or `-DDEBUG=2` (verbose) to enable
  - the source code is commented to explain the code
  - passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`
  - compile with `cc -O2 -o tinylisp tinylisp-extras-gc.c -lreadline`

See also [#20](https://github.com/Robert-van-Engelen/tinylisp/issues/20)

