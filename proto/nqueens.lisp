; n-queens example for tinylisp-extras-gc
; requires common.lisp

; some list.lisp definitions w/o loading list.lisp

(define nthcdr
    (lambda (t n)
        (if (eq? n 0)
            t
            (nthcdr (cdr t) (- n 1)))))

(define nth (lambda (t n) (car (nthcdr t n))))

(define seq-prepend
    (lambda (t n m)
        (if (< n m)
            (seq-prepend (cons (- m 1) t) n (- m 1))
            t)))
(define seq (lambda (n m) (seq-prepend () n m)))

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

; supporting functions

(define make-list
    (lambda (n x)
        (if (< 0 n)
            (cons x (make-list (- n 1) x))
            ())))

; n-queens solver

(define make-board
    (lambda (size)
        (mapcar
            (lambda () (make-list size '-))
            (seq 0 size))))

(define at
    (lambda (board x y)
        (nth (nth board x) y)))

(define queen!
    (lambda (board x y)
        (set-car! (nthcdr (nth board x) y) '@)))

(define clear!
    (lambda (board x y)
        (set-car! (nthcdr (nth board x) y) '-)))

(define queen?
    (lambda (board x y)
        (eq? (at board x y) '@)))

(define show
    (lambda (board)
        (if board
            (begin
                (println (car board))
                (show (cdr board)))
            ())))

(define conflict?
    (lambda (board x y)
        (any?
            (lambda (n)
	        (or ; check if there's no conflicting queen up
	            (queen? board n y)
                    ; check upper left
                    (let*
                        (z (+ y (- n x)))
                        (if (not (< z 0))
                            (queen? board n z)
                            ()))
                    ; check upper right
                    (let*
                        (z (+ y (- x n)))
                        (if (< z board-size)
                            (queen? board n z)
                            ()))))
            (seq 0 x))))

(define solve-n
    (lambda (board x)
        (if (eq? x board-size)
            ; show solution
            (begin
                (show board)
	        (println))
            ; continue searching for solutions
            (mapcar
	        (lambda (y)
		     (if (not (conflict? board x y))
                         (begin
                             (queen! board x y)
                             (solve-n board (+ x 1))
                             (clear! board x y))
                         ()))
                (seq 0 board-size)))))

(define solve
    (lambda (board)
        (begin
            (println 'start)
            (solve-n board 0)
            (println 'done))))

; create an 8x8 board and solve it

(define board-size 8)
(define board (make-board board-size))
(solve board)
