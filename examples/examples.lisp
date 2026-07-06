; examples for tinylist-extras (tinylisp with article's extras)
; requires list.lisp (load it first!)

; compute the dot product of (1 2 3) and (4 5 6) using map
(let*
    (prod (map * '(1 2 3) '(4 5 6)))
    (+ . prod))

; compute the dot product of (1 2 3) and (4 5 6) using map and foldl to sum
(foldl + 0 (map * '(1 2 3) '(4 5 6)))

; define a function to sum the result of mapping a function on list(s):
(define sum
    (lambda (f . args)
        (let*
            (t (map f . args))
            (+ . t))))

; compute the dot product of (1 2 3) and (4 5 6)
(sum * '(1 2 3) '(4 5 6))

; define function map-reduce to map function f then reduce with function g
(define map-reduce
    (lambda (f g . args)
        (let*
            (t (map f . args))
            (g . t))))

; compute the dot product of (1 2 3) and (4 5 6)
(map-reduce * + '(1 2 3) '(4 5 6))

; multiply the first values of three lists
(map-reduce car * '((1 2 3) (2 3 4) (3 4 5)))

; fetch letters from four lists by the index in the first list and list them
(map-reduce nth list '(1 1 2 3) '((a b c d) (e f g h) (i j k l) (m n o p)))

; tetch lists from three lists by index and or them to find if they have a non-()
(map-reduce nth or '(1 2 3)
    '( (() ())
       (() () 1)
       (() () () ()))
    )

; zip three lists (transpose) and append them together
(map-reduce list append '(1 2 3) '(4 5 6) '(7 8 9))

; define a macro-defining macro
(define defmacro
    (macro (f v x)
        `(define ,f (macro ,v ,x))))

; define a function-defining macro
(defmacro defun (f v x)
    `(define ,f (lambda ,v ,x)))

; define a macro to destructively swap-in a new value x for a variable v, returning its old value
(defmacro swap! (v x)
    `(let* (_ ,v)
        (progn
            (setq ,v ,x)
            _)))

; let's use it by iteratively computing the 7'th Fibonacci number
(let*
    (n 7)
    (a 0)
    (b 1)
    (while
        (< 0 (setq n (- n 1)))
        (setq b (+ (swap! a b) b))))

; and a list s of Fibonacci numbers iteratively constructed using swap!, set-cdr!, and setq on the tail node t of s
(let*
    (n 7)
    (a 0)
    (b 1)
    (s '(1))
    (t s)
    (progn
        (while
            (< 0 (setq n (- n 1)))
            (set-cdr! t (cons (setq b (+ (swap! a b) b)) ()))
            (setq t (cdr t)))
        s))

; generalize the construction of a list of Fibonacci numbers in a new fibo function:
(defun fibo (n)
    (let*
        (a 0)
        (b 1)
        (s (list 1))
        (t s)
        (progn
            (while
                (< 0 (setq n (- n 1)))
                (set-cdr! t (cons (setq b (+ (swap! a b) b)) ()))
                (setq t (cdr t)))
            s)))

; the first 100 Fibonacci numbers
(fibo 100)

; Macros such as (when ...) and (unless ...) defined below are expanded at
; runtime when used in a function body, so they incur overhead.  But defun and
; defmacro are macros that expand to a definition that is added to the
; environment, which incurs no overhead.

; (when <test> <expr1> <expr2> ... <exprn>) -- if <test> is true (i.e. not ()) then evaluate all <expr>
(defmacro when (x . args) `(if ,x (progn . ,args) ()))

; (unless <test> <expr1> <expr2> ... <exprn>) -- if <test> is false (i.e. ()) then evaluate all <expr>
(defmacro unless (x . args) `(if ,x () (progn . ,args)))

; we can reveal the source of a function definition by retrieving its Lisp expression

; display the definition of a function as a lambda expression (must be a closure)
(defun de-fun (f) (cons 'lambda (cons (car (car f)) (cons (cdr (car f)) ()))))

; display the fibo function as a lambda expression
(de-fun fibo)

; define a macro that expands into n copies of print statements
; (defmacro multi-print (n . args)
;     `(if ,(< 0 n)
;          (progn
;              (print . ,args)
;              (multi-print ,(- n 1) . ,args))
;          ()))
; the same can be achieved with letrec recursion (recommended) instead of a slow recursive macro:
(defmacro multi-print (n . args)
    (letrec*
        (c (lambda (k)
            (if (< 0 k)
                 `(progn
                     (print . ,args)
                     ,(c (- k 1) . args))
                 ())))
        (c n)))

; expand 10 print statements each with a distinct number from 1 to 10 to print with a space
(defun countup ()
    (let* (k 0)
        (multi-print 10 (setq k (+ k 1)) " ")))

; try it out
(countup)

; (dolist (<var> <list>) <expr1> ... <exprn>) loop <var> over <list> elements to execute <expr>
(defmacro dolist (x . args)
    `(let*                              ; (let*
        (,(car x) ())                   ;     (<var> ())
        (_ ,(car (cdr x)))              ;     (_ <list>)
        (while _                        ;     (while _
            (setq ,(car x) (car _))     ;         (setq <var> (car _))
            (setq _ (cdr _))            ;         (setq _ (cdr _))
            . ,args)))                  ;         <expr1> ... <exprn>)))

; try it out, we use (list ...) with "-atoms that are quoted, since '("foo" "bar") gives (quote ((quote foo) (quote bar)))
(dolist (v (list "Hello" "Lisp" "World")) (print v " "))

; backquote is a handy construct, not only for macros, but anytime when we need a (list ...) of things
(defun greet (name)
    (dolist (v `("Hello" ,name "--" ,name "is" "alive!")) (print v " ")))

(greet "Johnny 5")

; (case <expr> (<key1> <expr1>) (<key2> <expr2>) ... (<keyn> <exprn>)) match <key> to return value of corresponding <expr>
(defmacro case (x . args)
    (letrec*
        (c (lambda (t)
            (if t
                `(((eq? _ ,(car (car t))) ,(car (cdr (car t)))) . ,(c (cdr t)))
                `((#t ())))))
        `(let* (_ ,x) (cond . ,(c args)))))
; (case ...) is converted by a local recursive function (c args) that iterates over args to construct the
; (let* (_ <expr>) (cond ((eq? _ <key1>) <expr1>) ... (#t ())) conditions ((eq? _ <key>) <expr>) for each <key> <expr>

; try it out
(case 2
    (1 "first")
    (2 "second")
    (3 "third"))
