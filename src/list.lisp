(define length-tr
    (lambda (t n)
        (if t
            (length-tr (cdr t) (+ n 1))
            n)))
(define length (lambda (t) (length-tr t 0)))
(define append1
    (lambda (s t)
        (if s
            (cons (car s) (append1 (cdr s) t))
            t)))
(define append
    (lambda (t . args)
        (if args
            (append1 t (append . args))
            t)))
(define nthcdr
    (lambda (t n)
        (if (eq? n 0)
            t
            (nthcdr (cdr t) (- n 1)))))
(define nth (lambda (t n) (car (nthcdr t n))))
(define reverse-tr
    (lambda (r t)
        (if t
            (reverse-tr (cons (car t) r) (cdr t))
            r)))
(define reverse (lambda (t) (reverse-tr () t)))
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
            (lambda (x y) (if (< x y) x y))
            inf
            args)))
(define max
    (lambda args
        (foldl
            (lambda (x y) (if (< x y) y x))
            -inf
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
            (if (f (car t))
                (all? f (cdr t))
                ())
            #t)))
(define any?
    (lambda (f t)
        (if t
            (if (f (car t))
                #t
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
(define seq-prepend
    (lambda (t n m)
        (if (< n m)
            (seq-prepend (cons (- m 1) t) n (- m 1))
            t)))
(define seq (lambda (n m) (seq-prepend () n m)))
(define seqby-prepend
    (lambda (t n m k)
        (if (< 0 (* k (- m n)))
            (seqby-prepend (cons (- m k) t) n (- m k) k)
            t)))
(define seqby (lambda (n m k) (seqby-prepend () n (+ n k (* k (int (/ (- m n (if (< 0 k) 1 -1)) k)))) k)))
(define range
    (lambda (n m . args)
        (if args
            (seqby n m (car args))
            (seq n m))))
