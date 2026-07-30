; examples for tinylist-extras (tinylisp with article's extras) and tinylisp-extras-gc
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

; define a function that takes a function f to return its complement function
(define complement (lambda (f) (lambda args (not (f . args)))))

; which numbers in two lists are pairwise not equal?
(map (complement =) '(1 2 3 4) '(1 3 2 4))

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

; define a macro to shift variables like the shift command of shells such as bash, but with named Lisp variables
(defmacro shift v
    (letrec*
        (cc (lambda (_)
            (if (cdr _)
                (cons `(setq ,(car _) ,(car (cdr _))) (cc (cdr _)))
                ())))
        `(progn . ,(cc v))))

; locally assign a=1 b=2 c=3 then shift a<-b b<-c c<-4 to get a=2 b=3 c=4
(let* (a 1) (b 2) (c 3)
    (progn
        (shift a b c 4)
        (println "a=" a " b=" b " c=" c)))

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
        (cc (lambda (k)
            (if (< 0 k)
                 `(progn
                     (print . ,args)
                     ,(cc (- k 1) . args))
                 ())))
        (cc n)))

; expand 10 print statements each with a distinct number from 1 to 10 to print with a space
(defun countup ()
    (let* (k 0)
        (multi-print 10 (setq k (+ k 1)) " ")))

; try it out
(countup)

; (dolist (<var> <list>) <expr> ... <expr>) loop <var> over <list> elements to execute <expr>
(defmacro dolist (x . args)
    `(let*                              ; (let*
        (,(car x) ())                   ;     (<var> ())
        (_ ,(car (cdr x)))              ;     (_ <list>)
        (while _                        ;     (while _
            (setq ,(car x) (car _))     ;         (setq <var> (car _))
            (setq _ (cdr _))            ;         (setq _ (cdr _))
            . ,args)))                  ;         <expr> ... <expr>)))

; try it out, we use (list ...) with "-atoms that are quoted, since '("foo" "bar") gives (quote ((quote foo) (quote bar)))
(dolist (v (list "Hello" "Lisp" "World")) (print v " "))

; (foreach (<var> <list> ... <list>) <expr> ... <expr>) loops <var> over each element in each <list>
(defmacro foreach (x . args)
    (letrec*
        (v (car x))
        (cc (lambda (t)
            (if t
                (cons
                    `(let*
                        (,v ())
                        (_ ,(car t))
                        (while _
                            (setq ,v (car _))
                            (setq _ (cdr _))
                            . ,args))
                    (cc (cdr t)))
                ())))
        (cons 'progn (cc (cdr x)))))

; try it out
(foreach (v (list "Hello" "Lisp") (list "World")) (print v " "))

; (for (<var> <from> <to> [<step>]) <expr> ... <expr>) loops <var> from <from> to <to>, and by <step> when given
(defmacro for (x . args)
    (let*
        (v (car x))
        (a (car (cdr x)))
        (b (car (cdr (cdr x))))
        (if (cdr (cdr (cdr x)))
            (let* (s (car (cdr (cdr (cdr x)))))
                `(let* (,v (- ,a ,s)) (_ (* ,s ,b))
                     (while (not (< _ (* ,s (setq ,v (+ ,v ,s))))) . ,args)))
            `(let* (,v (- ,a 1)) (_ (+ ,b 1))
                 (while (< (setq ,v (+ ,v 1)) _) . ,args)))))

; try it out
(for (i 1 10 3) (print (* i i) " "))

; backquote is a handy construct, not only for macros, but anytime when we need a (list ...) of things
(defun greet (name)
    (dolist (v `("Hello" ,name "--" ,name "is" "alive!")) (print v " ")))

(greet "Johnny 5")

; (case <expr> (<key1> <expr1>) (<key2> <expr2>) ... (<keyn> <exprn>)) match <key> to return value of corresponding <expr>
(defmacro case (x . args)
    (letrec*
        (cc (lambda (t)
            (if t
                (if (eq? (car (car t)) 'otherwise)
                    `((#t ,(car (cdr (car t)))))
                    `(((eq? _ ',(car (car t))) ,(car (cdr (car t)))) . ,(cc (cdr t))))
                `((#t ())))))
        `(let* (_ ,x) (cond . ,(cc args)))))
; (case ...) is converted by a local recursive case-compiling function (cc args) that iterates over args to construct the
; (let* (_ <expr>) (cond ((eq? _ <key1>) <expr1>) ... (#t ())) conditions ((eq? _ <key>) <expr>) for each <key> <expr>
; note that <key> are constants that are not evaluated, this quoting of constants is done with (eq? _ ',(car (car t)))

; try it out
(case 2
    (1 "first")
    (2 "second")
    (3 "third")
    (otherwise "nothing"))

; tinylisp names may contain punctuation and digits, may start with a digit, but may not start with a , ( ) ' ` " ;
(defmacro 2nd-cdr (t) `(cdr ,t))
(defmacro 3rd-cdr (t) `(cdr (cdr ,t)))
(defmacro 4th-cdr (t) `(cdr (cdr (cdr ,t))))
(defmacro 5th-cdr (t) `(cdr (cdr (cdr (cdr ,t)))))
(defmacro 6th-cdr (t) `(cdr (cdr (cdr (cdr (cdr ,t))))))
(defmacro 7th-cdr (t) `(cdr (cdr (cdr (cdr (cdr (cdr ,t)))))))
(defmacro 1st (t) `(car ,t))
(defmacro 2nd (t) `(car (2nd-cdr ,t)))
(defmacro 3rd (t) `(car (3rd-cdr ,t)))
(defmacro 4th (t) `(car (4th-cdr ,t)))
(defmacro 5th (t) `(car (5th-cdr ,t)))
(defmacro 6th (t) `(car (6th-cdr ,t)))
(defmacro 7th (t) `(car (7th-cdr ,t)))

; try it out by generating a list of 1 to 9 then overwrite the 3rd element with 111
(let*
    (list (seq 1 10))
    (over (set-car! (3rd-cdr list) 111))
    list)
; note above that a let-local name may be any name, including a built-in name, used in the local lexical scope

; define a macro fn as a shorthand for a local (recursive) function f with arguments v and body x, used in expression y
(defmacro fn (f v x y)
    `(letrec* (,f (lambda ,v ,x)) ,y))

(defun factorial (n)
    (fn fact-tr (n k)
            (if (< 1 n)
                (fact-tr (- n 1) (* n k))
                k)
        (fact-tr n 1)))

; display the factorial function and try it out to compute 7! = 5040
(de-fun factorial)
(factorial 7)

; return multiple values from a function (as a list)
(defun foobar (name) (list 'foo 'bar name))

; define a macro to bind the return values to local variables v with (bind <funapp> (v1 v2 ... vk) <expr>)
(defmacro bind (x v y) `(let* (_ ,x) ((lambda ,v ,y) . _)))
          
; (foobar 'baz) returns (foo bar baz) bind this to (first second third) then (println first second third)
(bind (foobar 'baz) (first second third) (println first second third))
