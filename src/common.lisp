(define null? not)
(define err? (lambda (x) (eq? x 'ERR)))
(define number? (lambda (x) (eq? (* 0 x) 0)))
(define symbol?
    (lambda (x)
        (and
            x
            (not (err? x))
            (not (number? x))
            (not (pair? x)))))
(define atom?
    (lambda (x)
        (or
            (not x)
            (symbol? x))))
(define list?
    (lambda (x)
        (if (pair? x)
            (list? (cdr x))
            (not x))))
(define equal?
    (lambda (x y)
        (or
            (eq? x y)
            (and
                (pair? x)
                (pair? y)
                (equal? (car x) (car y))
                (equal? (cdr x) (cdr y))))))
(define negate (lambda (n) (- 0 n)))
(define > (lambda (x y) (< y x)))
(define <= (lambda (x y) (not (< y x))))
(define >= (lambda (x y) (not (< x y))))
(define = (lambda (x y) (eq? (- x y) 0)))
(define list (lambda args args))
(define cadr (lambda (x) (car (cdr x))))
(define caddr (lambda (x) (car (cdr (cdr x)))))
(define begin (lambda (x . args) (if args (begin . args) x)))
