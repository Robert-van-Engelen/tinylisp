; tail-recursive factorial function (converted from linear recursive)
(define fact
    (lambda (n)
        (letrec*
             (f (lambda (r k)
                 (if (< k 2)
                     r
                     (f (* r k) (- k 1)))))
             (f 1 n))))

; standard recursive factorial function is not tail-recursive
(define dumbfact
    (lambda (n)
        (if (< n 2)
            1
            (* n (dumbfact (- n 1))))))
