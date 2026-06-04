; test list.lisp

(define xs (list 1 2 3 4))

(or (equal? (length xs) 4) 'length)

(or (equal? (append xs xs xs) '(1 2 3 4 1 2 3 4 1 2 3 4)) 'append)

(or (equal? (nthcdr 2 xs) '(3 4)) 'nthcdr)

(or (equal? (nth 2 xs) 3) 'nth)

(or (equal? (reverse xs) '(4 3 2 1)) 'reverse)

(or (equal? (member 3 xs) '(3 4)) 'member)

(or (equal? (foldr * 1 xs) 24) 'foldr)

(or (equal? (foldl * 1 xs) 24) 'foldl)

(or (equal? (min . xs) 1) 'min)

(or (equal? (max . xs) 4) 'max)

(or (equal? (filter (lambda (x) (eq? (* 2 (int (/ x 2))) x)) xs) '(2 4)) 'filter)

(or (all? (lambda (x) (< 0 x)) xs) 'all?)

(or (any? (lambda (x) (< 3 x)) xs) 'any?)

(or (equal? (map * xs xs) '(1 4 9 16)) 'map)

(or (equal? (zip xs xs xs xs) '((1 1 1 1) (2 2 2 2) (3 3 3 3) (4 4 4 4))) 'zip)

(or (equal? (seq 1 5) '(1 2 3 4)) 'seq)

(or (equal? (seqby 1 5 2) '(1 3)) 'seqby)

(or (equal? (seqby -1 -5 -2) '(-1 -3)) 'seqby)
