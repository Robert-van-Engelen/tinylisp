; The article explains that tinylisp does not need an apply function that other
; Lisp need to apply a function to arguments and a list of additional arguments.

; Apparently, that was surprising as the literature often claims that Lisp
; requires an apply primitive as there is no way Lisp can define one in itself.

; Apply <func> to arguments <expr> and <list-expr> as a list of additional arguments:
; (apply <func> <expr1> <expr2> ... <exprk> <list-expr>)

; With tinylisp we simply use the dot operator.  In the case that <list-expr>
; is a variable holding the list of additional arguments:
; (<func> <expr1> <expr2> ... <exprk> . <var>)
; When <list-expr> is an expression then we use a let* to bind <var> first:
; (let* (args <list-expr>) (<func> <expr1> <expr2> ... <exprk> . args))

; We can also define a macro to do all this construction work automatically for you:

(define apply
    (macro fargs
        (letrec*
            (last (lambda (t)
                (if (cdr t)
                    (last (cdr t))
                    t)))
            (app (lambda (t s)
                (if (cdr t)
                    (cons (car t) (app (cdr t) s))
                    s)))
            (list 'let* (cons '_ (last fargs)) (app fargs '_)))))
