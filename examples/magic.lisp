; magic squares for tinylisp-extras and tinylisp-extras-gc

; requires the article's extras setq, println, progn, while
; run this in  tinylisp-extras-gc for large magic squares (with GC)

; modulus definition from math.lisp

(define mod (lambda (n m) (- n (* m (int (/ n m))))))

; some definitions from list.lisp

(define nthcdr
    (lambda (n t)
        (if (eq? n 0)
            t
            (nthcdr (- n 1) (cdr t)))))

(define nth (lambda (n t) (car (nthcdr n t))))

(define mapcar
    (lambda (f t)
        (if t
            (cons (f (car t)) (mapcar f (cdr t)))
            ())))
(define seq-prepend
    (lambda (t n m)
        (if (< n m)
            (seq-prepend (cons (- m 1) t) n (- m 1))
            t)))
(define seq (lambda (n m) (seq-prepend () n m)))

(define make-list
    (lambda (n x)
        (if (< 0 n)
            (cons x (make-list (- n 1) x))
            ())))

; magic squares

; wrap-around increment/decrement n within m
(define inc-wrap (lambda (n m) (mod (+ n 1) m)))
(define dec-wrap (lambda (n m) (mod (+ n m -1) m)))

; global data
(define square-size 0)
(define square ())

; make an empty square
(define make-square
    (lambda ()
        (mapcar
            (lambda () (make-list square-size 0))
            (seq 0 square-size))))

; set square at (row,col) with key value
(define square!
    (lambda (row col key)
        (set-car! (nthcdr col (nth row square)) key)))

; true if square at (row,col) is set
(define square?
    (lambda (row col)
        (< 0 (nth col (nth row square)))))

; populate the magic square
(define build-square
    (lambda ()
        (let*
            (row 0)
            (col (/ (- square-size 1) 2))
            (key 0)
            (while (< key (* square-size square-size))
                (setq key (+ key 1))
                (square! row col key)
                (let*
                    (row-1 (dec-wrap row square-size))
                    (col-1 (dec-wrap col square-size))
                    (if (square? row-1 col-1)
                        (setq row (inc-wrap row square-size))
                        (progn
                            (setq row row-1)
                            (setq col col-1))))))))

; display the magic square
(define show-square
    (lambda ()
        (mapcar println square)))

; create a new magic square -- size must be an odd number!!
(define magic
    (lambda (size)
        (progn
            (setq square-size size)
            (setq square (make-square size))
            (build-square)
            (show-square)
            'done)))

(magic 5)
