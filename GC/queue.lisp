; Queues in Lisp implemented with destructive operations using set-car! and set-cdr!
; Requires function last from list.lisp (load list.lisp)
;
; IMPORTANT: this example requires garbage collection to retain global tree data,
; otherwise queues will be corrupted when returning to the REPL with the trivial gc()
;
; A queue is represented by a pair (head . tail) where tail "points" to the last singleton list of the queue.
; The tail "pointer" is updated by enqueue.

(load list.lisp)

; return a new empty queue
(define queue (lambda () '(())))

; push value x to the back of the queue q, updating q, returns a singleton list (x)
(define enqueue
    (lambda (q x)
        (if (car q)
            (progn
                (set-cdr! (cdr q) (cons x ()))
                (set-cdr! q (cdr (cdr q))))
            (let* (t (list x))
                  (progn
                      (set-car! q t)
                      (set-cdr! q t))))))

; remove the front value from the queue q and return it
(define dequeue
    (lambda (q)
        (if (car q)
            (let* (x (car (car q)))
                (progn
                    (if (set-car! q (cdr (car q)))
                        ()
                        (set-cdr! q ()))
                    x))
            ())))

; (queued q) returns () when queue q is empty, otherwise returns a list of queued values
; the returned list will be shared by the queue, enqueued values are also added to the list
; use (copy-list (queued q)) to duplicate list t from the queue without sharing
(define queued car)

; convert list to queue in O(1) time, list will be shared by the queue, enqueued values are also added to the list
; use (list2queue (copy-list t)) to duplicate list t as a new queue without sharing
(define list2queue
    (lambda (t)
       (if t
           (cons t (last t))
           (queue))))

; example
(define Q (queue))
(enqueue Q 'hello)
(enqueue Q 'world)
(queued Q)              ; (hello world)
(dequeue Q)             ; hello
(dequeue Q)             ; world
(queued Q)              ; ()
