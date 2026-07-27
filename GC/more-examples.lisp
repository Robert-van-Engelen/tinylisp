; more examples for tinylisp-extras-expand-gc that includes more-extras.c additional built-in primitives
; see also examples.lisp

(load list.lisp)

; define a macro-defining macro
(define defmacro
    (macro (f v x)
        `(define ,f (macro ,v ,x))))

; define a function-defining macro
(defmacro defun (f v x)
    `(define ,f (lambda ,v ,x)))

; define a function to convert an atom name to a list of ASCII character codes
(defun atom2list (a)
    (foldr
        (lambda (n t) (cons (code a n) t))
        ()
        (seq 0 (clen a))))

; define a function to convert a list of ASCII character codes to an atom name
(defun list2atom (t) (atomize (map char t)))

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

; define a function to display a circle with radius r
(defun circle (r)
    (progn
        (for (k 0 r)
            (let (x (round (* r (sin (acos (- 1 (/ k r)))))))
                (println (char 32 (- r x)) (char 42 (* 2 x)))))
        (for (k 0 r)
            (let (x (round (* r (sin (acos (/ k r))))))
                (println (char 32 (- r x)) (char 42 (* 2 x)))))))

; display a green circle with radius 20 
(char 27) "[32;1m"      ; \e[32;1m bright green
(circle 20)
(char 27) "[35;1m"      ; \e[33;1m bright magenta
"this is a circle with r = 20"
(char 27) "[m"          ; \e[m normal

; define pi/2 and pi as a numeric values
(define pi/2 (acos 0))
"=" pi/2
(define pi (* 2 pi/2))
"=" pi
