; towers of Hanoi, moving n disks from src to dest, with spare available

; requires common.lisp for list and begin

; solve towers of Hanoi

(define hanoi
    (lambda (n src dest spare)
        (if (eq? n 1)
            (println (list 'Move 'from src 'to dest))
            (begin
                (hanoi (- n 1) src spare dest)
                (hanoi 1 src dest spare)
                (hanoi (- n 1) spare dest src)))))


; moving stack of size 1 from left to right with middle as spare
(hanoi 1 'left 'right 'middle)

; moving stack of size 2 from left to right with middle as spare
(hanoi 2 'left 'right 'middle)

; moving stack of size 3 from left to right with middle as spare
(hanoi 3 'left 'right 'middle)

; moving stack of size 4 from left to right with middle as spare
(hanoi 4 'left 'right 'middle)
