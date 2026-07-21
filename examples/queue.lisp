; Queues in Lisp implemented with destructive operations using set-car! and set-cdr!
; Requires function last from list.lisp (load list.lisp)

; return a new empty queue
(define queue (lambda () '(() ())))

; push value x to the back of the queue q to updating it, returns a singleton list (x)
(define enqueue
    (lambda (q x)
        (if (car q)
            (progn
                (set-cdr! (car (cdr q)) (cons x ()))
                (set-car! (cdr q) (cdr (car (cdr q)))))
            (let* (t (list x))
                  (progn
                      (set-car! q t)
                      (set-car! (cdr q) t))))))

; remove the front value from the queue q and return it
(define dequeue
    (lambda (q)
        (if (car q)
            (let* (x (car (car q)))
                (progn
                    (if (set-car! q (cdr (car q)))
                        ()
                        (set-car! (cdr q) ()))
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
           (list t (last t))
           (queue))))

; example
(define Q (queue))
(enqueue Q 'hello)
(enqueue Q 'world)
(queued Q)              ; (hello world)
(dequeue Q)             ; hello
(dequeue Q)             ; world
(queued Q)              ; ()
