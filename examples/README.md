Some of the tinylisp examples may require some additional Lisp features to run.  These features are explained in the article.  The `cell` memory may also have to be increased beyond 1K to work with larger inputs.

- ;-comments (only a single line change to tinylisp, see the article)
- apply.lisp uses `macro` and `letrec*`
- fact.lisp uses `letrec*` in one of the definitions
- hanoi.lisp uses `println` to output moves
