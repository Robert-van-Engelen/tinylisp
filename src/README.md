# Project code

Lisp in 99 lines is written in a Lisp-like functional style of structured C, lines are 55 columns wide on average and never wider than 120 columns for convenient editing.  It supports double precision floating point, has 20 built-in Lisp primitives, a REPL and garbage collection.  Tail-call optimized versions are included for speed and reduced memory use.

- [tinylisp.c](tinylisp.c) Lisp in 99 lines of C with double precision
- [tinylisp-commented.c](tinylisp-commented.c) commented version in an (overly) verbose C style
- [tinylisp-opt.c](tinylisp-opt.c) optimized version for speed and reduced memory use
- [tinylisp-float.c](tinylisp-float.c) Lisp in 99 lines of C with single precision
- [tinylisp-float-opt.c](tinylisp-float-opt.c) optimized version with single precision
- [lisp850.c](lisp850.c) Lisp in 99 lines of C for the Sharp PC-G850 with BCD boxing
- [lisp850-opt.c](lisp850-opt.c) optimized version for speed and reduced memory use
- [common.lisp](common.lisp) common Lisp functions defined in tiny Lisp itself
- [list.lisp](list.lisp) list functions library
- [math.lisp](math.lisp) some Lisp math functions
