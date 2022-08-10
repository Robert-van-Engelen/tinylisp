(define length
    (lambda (t)
        (if t
            (+ 1 (length (cdr t)))
            0)))
(define append
    (lambda (s t)
        (if s
            (cons (car s) (append (cdr s) t))
            t)))
(define nthcdr
    (lambda (t n)
        (if (eq? n 0)
            t
            (nthcdr (cdr t) (- n 1)))))
(define nth (lambda (t n) (car (nthcdr t n))))
(define rev1
    (lambda (r t)
        (if t
            (rev1 (cons (car t) r) (cdr t))
            r)))
(define reverse (lambda (t) (rev1 () t)))
(define member
    (lambda (x t)
        (if t
            (if (equal? x (car t))
                t
                (member x (cdr t)))
            t)))
(define foldr
    (lambda (f x t)
        (if t
            (f (car t) (foldr f x (cdr t)))
            x)))
(define foldl
    (lambda (f x t)
        (if t
            (foldl f (f (car t) x) (cdr t))
            x)))
(define min
    (lambda args
        (foldl
            (lambda (x y)
                (if (< x y)
                    x
                    y))
            9.999999999e99
            args)))
(define max
    (lambda args
        (foldl (lambda (x y)
            (if (< x y)
                y
                x))
        -9.999999999e99
        args)))
(define filter
    (lambda (f t)
        (if t
            (if (f (car t))
                (cons (car t) (filter f (cdr t)))
                (filter f (cdr t)))
            ())))
(define all?
    (lambda (f t)
        (if t
            (and
                (f (car t))
                (all? f (cdr t)))
            #t)))
(define any?
    (lambda (f t)
        (if t
            (or
                (f (car t))
                (any? f (cdr t)))
            ())))
(define mapcar
    (lambda (f t)
        (if t
            (cons (f (car t)) (mapcar f (cdr t)))
            ())))
(define map
    (lambda (f . args)
        (if (any? null? args)
            ()
            (let*
                (x (mapcar car args))
                (t (mapcar cdr args))
                (cons (f . x) (map f . t))))))
(define zip (lambda args (map list . args)))
(define seq
    (lambda (n m)
        (if (< n m)
            (cons n (seq (+ n 1) m))
            ())))
(define seqby
    (lambda (n m k)
        (if (< 0 (* k (- m n)))
            (cons n (seqby (+ n k) m k))
            ())))
(define range
    (lambda (n m . args)
        (if args
            (seqby n m (car args))
            (seq n m))))
