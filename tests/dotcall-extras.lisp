; check addition
(cons
    (if (equal?
            ((lambda (l) (+ . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(+))

; check subtraction
(cons
    (if (equal?
            ((lambda (l) (- . l)) '(1 2 3))
            -4)
        'passed
        'failed)
    '(-))

; check multiplication
(cons
    (if (equal?
            ((lambda (l) (* . l)) '(1 2 3))
            6)
        'passed
        'failed)
    '(*))

; check division
(cons
    (if (equal?
            ((lambda (l) (/ . l)) '(1 2))
            0.5)
        'passed
        'failed)
    '(/))

; check let*
(cons
    (if (equal?
            (let*
                (x 1) (y (+ 1 x))
                (let* (z (+ x y)) z))
            3)
        'passed
        'failed)
    '(let*))

; check let
(cons
    (if (equal?
            (let (x 1) (y 2)
              (let (z (+ x y)) z))
            3)
        'passed
        'failed)
    '(let))

; check letrec* recursion
(cons
    (if (equal?
            (let*
                (x 5)
                (letrec* (f (lambda (n) (if (< n 2) 1 (* n (f (- n 1)))))) (f x)))
            120)
        'passed
        'failed)
    '(letrec*))

; check letrec* tail-recursive consing in left argument
(cons
    (if (equal?
            (letrec* (f (lambda (xs n) (if (< n 1) xs (f (cons n xs) (- n 1))))) (f () 9))
            '(1 2 3 4 5 6 7 8 9))
        'passed
        'failed)
    '(letrec* consing left))

; check letrec* tail-recursive consing in right argument
(cons
    (if (equal?
            (letrec* (f (lambda (n xs) (if (< n 1) xs (f (- n 1) (cons n xs))))) (f 9 ()))
            '(1 2 3 4 5 6 7 8 9))
        'passed
        'failed)
    '(letrec* consing right))

; check letrec mutual recursion
(cons
    (if (equal?
            (let*
                (x 5)
                (letrec
                    (f (lambda (n) (if (< n 2) 1 (* n (g (- n 1))))))
                    (g (lambda (n) (if (< n 2) 1 (* n (f (- n 1))))))
                    (f x)))
            120)
        'passed
        'failed)
    '(letrec))

; check letrec tail-recursive consing in left argument
(cons
    (if (equal?
            (letrec (f (lambda (xs n) (if (< n 1) xs (f (cons n xs) (- n 1))))) (f () 9))
            '(1 2 3 4 5 6 7 8 9))
        'passed
        'failed)
    '(letrec consing left))

; check letrec tail-recursive consing in right argument
(cons
    (if (equal?
            (letrec (f (lambda (n xs) (if (< n 1) xs (f (- n 1) (cons n xs))))) (f 9 ()))
            '(1 2 3 4 5 6 7 8 9))
        'passed
        'failed)
    '(letrec consing right))

; check static scoping
(cons
    (if (equal?
            (let*
                (x 2)
                ((let* (f +) (x 1) (lambda (y) (f x y))) x))
            3)
        'passed
        'failed)
    '(static scoping))

; check curring with the dot operator
(cons
    (if (equal?
            (((lambda (f x)
                (lambda args (f x . args))) + 1) 2 3)
            6)
        'passed
        'failed)
    '(currying))

; check lambda argument passing
(cons
    (if (equal?
            ((lambda (x y z) (list x y z)) '(1) '(2) '(3))
            '((1) (2) (3)))
        'passed
        'failed)
    '(same args))

; check lambda argument passing with extra actual arguments
(cons
    (if (equal?
            ((lambda (x y) (list x y)) '(1) '(2) '(3))
            '((1) (2)))
        'passed
        'failed)
    '(extra args))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) (3)))
        'passed
        'failed)
    '(caller dot))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (x y . z) (list x y z)) '(1) '(2) '(3))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(callee dot))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) '(2) . l)) '((3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(both dot))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) '(2) . l)) '((3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(extra dot))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x . y) (list x y)) '(1) . l)) '((2) (3)))
            '((1) ((2) (3))))
        'passed
        'failed)
    '(scant dot))

; check lambda argument passing with the dot operator
(cons
    (if (equal?
            ((lambda (l)
                ((lambda (x y . z) (list x y z)) '(1) . l)) '((2) (3)))
            '((1) (2) ((3))))
        'passed
        'failed)
    '(early-dot))

; check macro expansion into Lisp code that delays code execution then force eval
(cons
    (if (equal?
            (let*
                (delay (macro (x) (list 'lambda () x)))
                (f (delay (cons 1 (cons 2 (cons 3 ())))))
                (f))
            '(1 2 3))
        'passed
        'failed)
    '(macro))

; assoc on a list of bindings
(cons
    (if (equal?
            (let*
                (a 1) (b 2) (e (list (list 'x . a) (list 'y . b)))
                (+ (assoc 'x e) (assoc 'y e)))
            3)
        'passed
        'failed)
    '(assoc))

; recurse 9 levels deep then throw an exception
(cons
    (if (equal?
            (letrec*
                (f (lambda (n) (if (eq? n 9) (throw 9) (f (+ n 1)))))
                (catch (f 1)))
            '(ERR . 9))
        'passed
        'failed)
    '(catch throw))

; loop with destructive setq (when defined) to calculate 5! = 120
(cons
    (if (equal? (catch setq) '(ERR . 2))
        'skip
        (if (equal?
                (letrec*
                    (n 5)
                    (r 1)
                    (f (lambda ()
                        (if (< n 1)
                            r
                            (begin (setq r (* r n)) (setq n (- n 1))  (f)))))
                    (f))
                120)
            'passed
            'failed))
        '(setq))

; destructive set-car! (when defined) to set all list elements to 1
(cons
    (if (equal? (catch setq) '(ERR . 2))
        'skip
        (if (equal?
                (letrec*
                    (xs (list 1 2 3))
                    (f (lambda (xs)
                        (if xs
                            (f (begin (set-car! xs 1) (cdr xs)))
                            ())))
                    (begin (f xs) xs))
                '(1 1 1))
            'passed
            'failed))
    '(set-car!))

'OK
