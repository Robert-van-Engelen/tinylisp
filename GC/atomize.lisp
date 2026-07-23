; example atomizations for tinylisp-extras-gc with new atomize primitive

; atomization rules:
;   atomize atom         => atom
;   atomize number       => atom with number in decimal text
;   atomize 'atom        => atom
;   atomize `atom        => atom
;   atomize "text"       => atom with given text that may contain spacing
;   atomize ()           => atomize " "
;   atomize x y ...      => concatenate atomize x atomize y atomize ...
;   atomize '(x y ...)   => concatenate atomize x atomize y atomize ...
;   atomize `(x y ...)   => concatenate atomize x atomize y atomize ...
;   atomize (f x y ...)  => atomize the result of the evaluation of (f x y ...)
;   atomize (progn atom) => atomize the value of the variable named atom
;
; atomize is used by:
;   (load pathname)          atomizes pathname of the file to open and load
;   (read pathname)          atomizes pathname of the file to open and read
;   (read)                   reads a Lisp expression from standaard input
;   (write-to pathname ...)  atomizes pathname of the file to open and write
;
; (write-to pathname <expr> <expr> ...)
;   redirect the print/ln statements in <expr> to the file created as pathname
;   example: (write-to output.txt (println "; environment") (println (env)))
;
; (write-to +pathname <expr> <expr> ...)
;   redirect the print/ln statements in <expr> to append to the file pathname

(atomize foo () bar -123)               ; same as atom "foo bar-123"

(atomize 'foo " " `bar -123)            ; same as atom "foo bar-123"

(let*
    (x 'foo)
    (y 'bar)
    (atomize (list x () y) -123))       ; same as atom "foo bar-123"

(let*
    (x 'foo)
    (s " ")
    (atomize `(,x ,s bar) -123))        ; same as atom "foo bar-123"
