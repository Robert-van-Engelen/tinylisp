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
(define map1
    (lambda (f t)
        (if t
            (cons (f (car t)) (map1 f (cdr t)))
            ())))
(define map
    (lambda (f . args)
        (if (any? null? args)
            ()
            (let*
                (mapcar (map1 car args))
                (mapcdr (map1 cdr args))
                (cons (f . mapcar) (map f . mapcdr))))))
(define zip (lambda args (map list . args)))
(define range
    (lambda (n m . args)
        (if args
            (if (< 0 (* (car args) (- m n)))
                (cons n (range (+ n (car args)) m (car args)))
                ())
            (if (< n m)
                (cons n (range (+ n 1) m))
                ()))))
