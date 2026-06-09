; binary trees for tinylisp-extras-gc - destructive implementation with set-car! and set-cdr!

(load list.lisp)

; a node is a triple ((key . val) bt-left bt-right) with key-val pairs (key . val)
; a leaf is ()
; a root is a singleton (root)
; for deletion, parent bt-left and bt-right are updated by nodes below using set-car!

(define    kv-of (lambda (bt) (car (car bt))))
(define   key-of (lambda (bt) (car (car (car bt)))))
(define   val-of (lambda (bt) (cdr (car (car bt)))))
(define  left-of (lambda (bt) (cdr (car bt))))
(define right-of (lambda (bt) (cdr (cdr (car bt)))))

; new empty binary tree
(define bt-new (lambda () (list ())))

; find the minimum key-val in the binary tree, defaults to the given k-v pair when the tree is empty
(define bt-min
    (lambda (bt kv)
        (if (car bt)
            (bt-min (left-of bt) (kv-of bt))
            kv)))

; find the maximum key-val in the binary tree, defaults to the given k-v pair when the tree is empty
(define bt-max
    (lambda (bt kv)
        (if (car bt)
            (bt-max (right-of bt) (kv-of bt))
            kv)))

; find the val of a key in the binary tree
(define bt-find
    (lambda (bt key)
        (if (car bt)
            (cond
                ((< key (key-of bt)) (bt-find (left-of bt) key))
                ((< (key-of bt) key) (bt-find (right-of bt) key))
                (#t (val-of bt)))
            ())))

; insert key with val in the binary tree
(define bt-ins
    (lambda (bt key val)
        (if (car bt)
            (cond
                ((< key (key-of bt)) (bt-ins (left-of bt) key val))
                ((< (key-of bt) key) (bt-ins (right-of bt) key val))
                (#t (set-cdr! (kv-of bt) val)))
        (set-car! bt (list (cons key val) () ())))))

; delete key from the binary tree, return its val
(define bt-del
    (lambda (bt key)
        (if (car bt)
            (cond
                ((< key (key-of bt)) (bt-del (left-of bt) key))
                ((< (key-of bt) key) (bt-del (right-of bt) key))
                (#t (let*
                        (kv (kv-of bt))
                        (progn
                            (if (car (left-of bt))
                                (if (car (right-of bt))
                                    (let*
                                        (min-kv (bt-min (right-of bt) kv))
                                        (progn
                                            (set-car! (car bt) min-kv)
                                            (bt-del (right-of bt) (car min-kv))))
                                    (set-car! bt (car (left-of bt))))
                                (set-car! bt (car (right-of bt))))
                            (cdr kv)))))
            ())))

; apply a function to each (key . val) pair in a binary tree, the function must not modify the tree
(define bt-walk
    (lambda (bt f)
        (if (car bt)
            (progn
                (bt-walk (left-of bt) f)
                (f (kv-of bt))
                (bt-walk (right-of bt) f))
            ())))

; add a list of (key . val) pairs to a binary tree
(define env->bt
    (lambda (bt env)
        (progn
            (map (lambda (kv) (bt-ins bt (car kv) (cdr kv))) env)
            ())))

; extract all (key . val) pairs from a binary tree, keeping the binary tree
(define bt->env
    (lambda (bt)
        (letrec*
            (ex (lambda (bt t) 
                    (if (car bt)
                        (ex (left-of bt) (cons (kv-of bt) (ex (right-of bt) t)))
                        t)))
            (ex bt ()))))
            

; TESTING

; Because set-car! and set-cdr! are applied to build and modify a binary tree
; that is part of the global environment, this requires GC and won't work with
; the simple tinylisp versions that destroy the tree when returning to the REPL

; create a new binary tree and populate it
(define btree (bt-new))
(env->bt btree (list '(4 . d) '(2 . b) '(1 . a) '(3 . c) '(6 . f) '(5 . e) '(7 . g)))

; println (key . val) pairs
(bt-walk btree println)

; get a list of all (key . val) pairs
(bt->env btree)

; get a list of all keys
(map car (bt->env btree))

; get a list of all values
(map cdr (bt->env btree))

; delete all nodes of the tree by extracting the keys then map bt-del on each key
(map (lambda (key) (bt-del btree key)) (map car (bt->env btree)))

; use setq to empty a binary tree
(setq btree (bt-new))
