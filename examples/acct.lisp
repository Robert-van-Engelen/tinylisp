; balancing an account
; from: https://sicp.sourceacademy.org/chapters/3.1.1.html
;       originally by Harold Abelson and Gerald Jay Sussman with Julie Sussman
;
; requires tinylisp-extras for begin, setq
;
; (load acct.lisp)
; (define acct1 (make-withdraw 50))
; (acct1 10)
; (acct1 10)
; ...
; insufficien funds

(define make-withdraw
    (lambda (balance)
        (lambda (amount)
            (if (>= balance amount)
                (begin
                    (setq balance (- balance amount))
                    balance)
                "insufficien funds"))))
