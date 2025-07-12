# Unit tests

The dot operator in lambda variable lists and in actual argument lists is tested for all combinationsin [dotcall.lisp](dotcall.lisp), arithmetic and logic is tested, and list operations are tested:

```console
$ ./tinylisp < dotcall.lisp
tinylisp
925>equal?
841>list
820>(passed same-args)
817>(passed extra-args)
816>(passed scant-args)
814>(passed caller-dot)
813>(passed callee-dot)
811>(passed both-dot)
810>(passed extra-dot)
809>(passed scant-dot)
808>(passed early-dot)
806>(passed builtin-add)
805>(passed builtin-sub)
803>(passed builtin-mul)
802>(passed builtin-div)
800>OK
```

When the article's additional Lisp features are implemented, then test with [dotcall-extras.lisp](dotcall-extras.lisp):

```console
$ ./tinylisp-extras
tinylisp
...
7431>(load dotcall-extras.lisp)
dotcall-extras.lisp
(passed same-args)
(passed extra-args)
(passed caller-dot)
(passed callee-dot)
(passed both-dot)
(passed extra-dot)
(passed scant-dot)
(passed early-dot)
(passed builtin-add)
(passed builtin-sub)
(passed builtin-mul)
(passed builtin-div)
(passed let*)
(passed let)
(passed letrec*)
(passed macro)
(passed assoc)
OK
```

