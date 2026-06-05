; n-queens example in Common Lisp e.g. for SBCL and CLISP

; Common Lisp isn't simply Lisp, it requires special forms such as funcall,
; (declare (ignore x)), and deffun, defvar, defconstant to sugar define.

; SBCL requires defconstant before the constant is used in the code below it.

; We keep some list function definitions herein instead of using Common Lisp
; built-in nth, mapcar, and make-list for a fair comparison to tinylisp.  While
; setf is nice to replace set-car with (setf (car (nthcdr1 ...))) it does not
; permit (setf (nth1 ...)) but (setf (nth ...)) with built-in nth works.
; Go figure.

(defconstant board-size 8)

(defun nthcdr1 (n s)
    (if (eq n 0)
        s
        (nthcdr1 (- n 1) (cdr s))))

(defun nth1 (n s) (car (nthcdr1 n s)))

(defun seq-prepend (s n m)
    (if (< n m)
        (seq-prepend (cons (- m 1) s) n (- m 1))
        s))
(defun seq (n m) (seq-prepend () n m))

(defun any? (f s)
    (if s
        (if (funcall f (car s))
            t
            (any? f (cdr s)))
        ()))

(defun mapcar1 (f s)
    (if s
        (cons (funcall f (car s)) (mapcar1 f (cdr s)))
        ()))

; supporting functions

(defun make-list1 (n x)
    (if (< 0 n)
        (cons x (make-list1 (- n 1) x))
        ()))

; n-queens solver

(defun make-board (size)
    (mapcar1
        (lambda (x) (declare (ignore x)) (make-list1 size '-))
        (seq 0 size)))

(defun at (board x y)
    (nth1 y (nth1 x board)))

(defun queen! (board x y)
    (setf (car (nthcdr1 y (nth1 x board))) '@))

(defun clear! (board x y)
    (setf (car (nthcdr1 y (nth1 x board))) '-))

(defun queen?  (board x y)
    (eq (at board x y) '@))

(defun show (board)
    (if board
        (progn
            (print (car board))
            (show (cdr board)))
        ()))

(defun conflict? (board x y)
    (any?
        (lambda (n)
            (or ; check if there's no conflicting queen up
                (queen? board n y)
                ; check upper left
                (let*
                    ( (z (+ y (- n x)))
                    )
                    (if (not (< z 0))
                        (queen? board n z)
                        ()))
                ; check upper right
                (let*
                    ( (z (+ y (- x n)))
                    )
                    (if (< z board-size)
                        (queen? board n z)
                        ()))))
        (seq 0 x)))

(defun solve-n (board x)
    (if (eq x board-size)
        ; show solution
        (progn
            (show board)
            (format t "~%"))
        ; continue searching for solutions
        (mapcar1
            (lambda (y)
                (if (not (conflict? board x y))
                    (progn
                        (queen! board x y)
                        (solve-n board (+ x 1))
                        (clear! board x y))
                    ()))
            (seq 0 board-size))))

(defun solve (board)
    (progn
        (format t "start~%")
        (solve-n board 0)
        (format t "done~%")))

; create an 8x8 board and solve it

(defvar board (make-board board-size))
(defun run () (solve board))

(run)

