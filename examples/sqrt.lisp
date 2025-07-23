; (sqrt n) -- solve x^2 - n = 0 with Newton method using the Y combinator to recurse
; ... we could add math.h sqrt() as a Lisp primitive, but what's the fun in that?

(define Y (lambda (f) (lambda args ((f (Y f)) . args))))

(define sqrt (lambda (n)
    ((Y (lambda (f)
            (lambda (x)
                (let*
                    (y (- x (/ (- x (/ n x)) 2)))
                    (if (eq? x y)
                        x
                        (f y))))))
     n)))
