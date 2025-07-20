# Project source code

Lisp in 99 lines is written in a Lisp-like functional style of structured C, lines are 55 columns wide on average and never wider than 120 columns for convenient editing.  It supports static scoping, double precision floating point, has 21 built-in Lisp primitives, a REPL and a simple garbage collector.  Tail-call optimized versions are included for speed and reduced memory use.

- [tinylisp.c](tinylisp.c) Lisp in 99 lines of C with double precision
- [tinylisp-commented.c](tinylisp-commented.c) commented version in an (overly) verbose C style
- [tinylisp-opt.c](tinylisp-opt.c) optimized version for speed and reduced memory use
- [tinylisp-float.c](tinylisp-float.c) Lisp in 99 lines of C with single precision
- [tinylisp-float-opt.c](tinylisp-float-opt.c) optimized version with single precision
- [lisp850.c](lisp850.c) Lisp in 99 lines of C for the Sharp PC-G850 with BCD boxing
- [lisp850-opt.c](lisp850-opt.c) optimized version for speed and reduced memory use
- [common.lisp](common.lisp) common Lisp functions defined in tiny Lisp itself
- [list.lisp](list.lisp) list functions library, requires common.lisp definitions
- [math.lisp](math.lisp) some Lisp math functions

TL;DR: the article's additions and optimizations fully implemented with comments, including section references:

- [tinylisp-extras.c](tinylisp-extras.c) compile with `-lreadline`

The extras version adds 15 Lisp primitives for Lisp source loading, readline, input and output Lisp expressions, exceptions, CTRL-C break, macros, and execution tracing.
