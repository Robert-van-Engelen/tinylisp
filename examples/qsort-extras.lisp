; (qsort <list>) -- quicksort
; For example, (qsort '(3 4 1 2 5)) => (1 2 3 4 5)

; requires tinylisp-extra Lisp primitives: setq, progn, while

(define qsort-tr
    (lambda (t s)
        (if t
            (let*
                ; pivot x
                (x (car t))
                ; less-than-pivot x accumulated in the while loop
                (l ())
                ; greater-or-equal-to-pivot x accumulated in the while loop
                (g ())
                (progn
                    (while (setq t (cdr t))
                        (if (< (car t) x)
                            (setq l (cons (car t) l))
                            (setq g (cons (car t) g))))
                    (qsort-tr l (cons x (qsort-tr g s)))))
            s)))
(define qsort (lambda (t) (qsort-tr t ())))
