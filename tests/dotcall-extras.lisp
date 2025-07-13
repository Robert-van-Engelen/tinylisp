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
    '(same-args))

(cons
    (if (equal?
            ((lambda (x y) (list x y)) '(1) '(2) '(3))
            '((1) (2)))
        'passed
        'failed)
    '(extra-args))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) (3)))
        'passed
        'failed)
    '(caller-dot))

(cons
    (if (equal?
            ((lambda (x y . z) (list x y z)) '(1) '(2) '(3))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(callee-dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(both-dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) '(2) . l)) '((3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(extra-dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) . l)) '((2) (3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(scant-dot))

(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) . l)) '((2) (3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(early-dot))

(cons
    (if (equal?
            ((lambda (l) (+ . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(builtin-add))

(cons
    (if (equal?
            ((lambda (l) (- . l)) '(1 2 3))
            -4)
        'passed
        'failed)
    '(builtin-sub))

(cons
    (if (equal?
            ((lambda (l) (* . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(builtin-mul))

(cons
    (if (equal?
            ((lambda (l) (/ . l)) '(1 2))
            0.5)
        'passed
        'failed)
    '(builtin-div))

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
            (let (x 1) (y 2)
              (let (z (+ x y)) z))
            3)
        'passed
        'failed)
    '(let))

(cons
    (if (equal?
            (let* (x 5)
                (letrec* (f (lambda (n) (if (< n 2) 1 (* n (f (- n 1)))))) (f x)))
            120)
        'passed
        'failed)
    '(letrec*))

(cons
    (if (equal?
            (let* (delay (macro (x) (list 'lambda () x))) (f (delay (+ 1 2))) (f))
            3)
        'passed
        'failed)
    '(macro))

(cons
    (if (equal?
            (let*
                (a 1) (b 2) (e (list (list 'x . a) (list 'y . b)))
                (+ (assoc 'x e) (assoc 'y e)))
            3)
        'passed
        'failed)
    '(assoc))

(cons
    (if (equal?
            (letrec*
                (f (lambda (n) (if (eq? n 9) (throw 9) (f (+ n 1)))))
                (catch (f 1)))
            '(ERR . 9))
        'passed
        'failed)
    '(catch-throw))

'OK
