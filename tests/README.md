# Unit tests

The dot operator in lambda variable lists and in actual argument lists is tested for all combinationsin [dotcall.lisp](dotcall.lisp), arithmetic and logic is tested, and list operations are tested:

```console
$ ./tinylisp < dotcall.lisp
tinylisp
925>equal?
841>list
820>(passed +)
818>(passed -)
818>(passed *)
818>(passed /)
818>(passed let*)
818>(passed static scoping)
816>(passed currying)
815>(passed same args)
814>(passed extra args)
813>(passed scant args)
813>(passed caller dot)
811>(passed callee dot)
810>(passed both dot)
810>(passed extra dot)
810>(passed scant dot)
810>(passed early dot)
809>OK
```

When the article's additional Lisp features are implemented, then test with [dotcall-extras.lisp](dotcall-extras.lisp):

```console
$ ./tinylisp-extras
tinylisp
...
7427>(load dotcall-extras.lisp)
dotcall-extras.lisp
(passed +)
(passed -)
(passed *)
(passed /)
(passed let*)
(passed let)
(passed letrec*)
(passed letrec* consing left)
(passed letrec* consing right)
(passed static scoping)
(passed currying)
(passed same args)
(passed extra args)
(passed caller dot)
(passed callee dot)
(passed both dot)
(passed extra dot)
(passed scant dot)
(passed early-dot)
(passed macro)
(passed assoc)
(passed catch throw)
(passed setq)
(passed set-car!)
OK
7427>
```

