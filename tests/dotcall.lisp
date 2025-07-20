(define equal?
    (lambda (x y)
        (or
            (eq? x y)
            (and
                (pair? x)
                (pair? y)
                (equal? (car x) (car y))
                (equal? (cdr x) (cdr y))))))

(define list (lambda args args))

(cons
    (if (equal?
            ((lambda (l) (+ . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(+))

(cons
    (if (equal?
            ((lambda (l) (- . l)) '(1 2 3))
            -4)
        'passed
        'failed)
    '(-))

(cons
    (if (equal?
            ((lambda (l) (* . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(*))

(cons
    (if (equal?
            ((lambda (l) (/ . l)) '(1 2))
            0.5)
        'passed
        'failed)
    '(/))

(cons
    (if (equal?
            (let*
                (x 1) (y (+ 1 x))
                (let* (z (+ x y)) z))
            3)
        'passed
        'failed)
    '(let*))

(cons
    (if (equal?
            (let*
                (x 2)
                ((let* (f +) (x 1) (lambda (y) (f x y))) x))
            3)
        'passed
        'failed)
    '(static scoping))

(cons
    (if (equal?
            (((lambda (f x)
                (lambda args (f x . args))) + 1) 2 3)
            6)
        'passed
        'failed)
    '(currying))

(cons
    (if (equal?
            ((lambda (x y z) (list x y z)) '(1) '(2) '(3))
            '((1) (2) (3)))
        'passed
        'failed)
    '(same args))

(cons
    (if (equal?
            ((lambda (x y) (list x y)) '(1) '(2) '(3))
            '((1) (2)))
        'passed
        'failed)
    '(extra args))

(cons
    (if (equal?
            ((lambda (x y z) (list x y z)) '(1) '(2))
            '((1) (2) ERR))
        'passed
        'failed)
    '(scant args))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) (3)))
        'passed
        'failed)
    '(caller dot))

(cons
    (if (equal?
            ((lambda (x y . z) (list x y z)) '(1) '(2) '(3))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(callee dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(both dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) '(2) . l)) '((3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(extra dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) . l)) '((2) (3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(scant dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) . l)) '((2) (3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(early dot))

'OK
