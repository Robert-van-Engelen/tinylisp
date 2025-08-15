**Tinylisp proto version with reference count garbage collection**

See [#20](https://github.com/Robert-van-Engelen/tinylisp/issues/20)

Passes `tests/dotcall-extras.lisp` tests and runs 8-queens `nqueens.lisp`.

Compile with `cc -O2 -o tinylisp tinylisp-extras-gc.c -lreadline`.
