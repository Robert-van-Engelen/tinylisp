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
      ((lambda (x y z) (list x y z)) '(1) '(2) '(3)) '((1) (2) (3)))
     'passed
     'failed)
 '(same-args))

(cons
 (if (equal?
      ((lambda (x y) (list x y)) '(1) '(2) '(3)) '((1) (2)))
     'passed
     'failed)
 '(extra-args))

(cons
 (if (equal?
      ((lambda (x y z) (list x y z)) '(1) '(2)) '((1) (2) ERR))
     'passed
     'failed)
 '(scant-args))

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
      ((lambda (l)
         (+ . l)) '(1 2 3))
      6)
     'passed
     'failed)
 '(builtin-add))

(cons
 (if (equal?
      ((lambda (l)
         (- . l)) '(1 2 3))
      -4)
     'passed
     'failed)
 '(builtin-sub))

(cons
 (if (equal?
      ((lambda (l)
         (* . l)) '(1 2 3))
      6)
     'passed
     'failed)
 '(builtin-mul))

(cons
 (if (equal?
      ((lambda (l)
         (/ . l)) '(1 2))
      0.5)
     'passed
     'failed)
 '(builtin-div))

'OK
