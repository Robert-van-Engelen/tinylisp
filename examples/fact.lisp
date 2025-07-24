; tail-recursive factorial function (converted from linear recursive)
(define fact
    (lambda (n)
        (letrec*
             (f (lambda (r k)
                 (if (< k 2)
                     r
                     (f (* r k) (- k 1)))))
             (f 1 n))))

; using the Y combinator for recursion without letrec* recursive naming
(define Y (lambda (f) (lambda args ((f (Y f)) . args))))
(define facty
    (lambda (n)
        ((Y (lambda (f)
                (lambda (k)
                    (if (< 1 k)
                        (* k (f (- k 1)))
                        1))))
         n)))

; using function seq defined in list.lisp and the dot operator
(define factseq
    (lambda (n)
        (let*
            (range (seq 2 (+ n 1)))
            (* 1 . range))))

; standard recursive factorial function is not tail-recursive
(define dumbfact
    (lambda (n)
        (if (< n 2)
            1
            (* n (dumbfact (- n 1))))))
