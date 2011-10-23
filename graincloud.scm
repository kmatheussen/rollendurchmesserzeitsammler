

(define get-blocksize (foreign-procedure () int "get_blocksize"))
(define out (foreign-procedure (int float) void "out_sample"))
(define block-ready (foreign-procedure () void "block_ready"))
(define between (foreign-procedure (float float) float "between"))
(define intbetween (foreign-procedure (int int) int "intbetween"))
(define debug (foreign-procedure (int) void "schemedebug"))

(define blocksize (get-blocksize))

(define volume 0.05)


(define (scale x x1 x2 y1 y2)
  (+ y1
     (/ (* (- x x1)
	   (- y2 y1))
	(- x2 x1))))


(define (make-oscil frequency seconds)
  (define phase 0.0)
  (define phaseinc (* frequency (/ (* 3.14159 2.0) 44100.0)))
  (define duration (* 44100.0 seconds))
  (define attack (/ duration 10))
  (define incmul (between 0.99998 1.0001))
  (define pos 0)
  (lambda ()
    (if (>= pos duration)
	-100.0
	(let ((ret (sin phase)))
	  (set! phase (+ phase phaseinc))
	  (set! phaseinc (* phaseinc incmul))
	  (set! pos (+ pos 1))
	  (* ret
	     (if (< pos attack)
		 (scale pos 0 attack 0.0 1.0)
		 (scale pos attack duration 1.0 0.0)))))))

(define instruments '())

(define (process i)
  (let loop ((instruments instruments)
	     (val 0.0))
    (if (null? instruments)
	(out i (* volume val))
	(let ((res ((car instruments))))
	  (if (>= res -1)
	      (loop (cdr instruments)
		    (+ val res))
	      (loop (cdr instruments)
		    val))))))

(define (process-remove i)
  (set! instruments
	(let loop ((instruments instruments)
		   (val 0.0))
	  (if (null? instruments)
	      (begin
		(out i (* volume val))
		'())
	      (let ((res ((car instruments))))
		(if (>= res -1.0)
		    (cons (car instruments)
			  (loop (cdr instruments)
				(+ val res)))
		    (loop (cdr instruments)
			  val)))))))

(define how-many 4)

(let block-loop ()
  (block-ready)
  (let loop ((i (intbetween 0 how-many)))
    (cond ((> i 0)
	   (set! instruments (cons (make-oscil (between 50.0 1000.0)
					       (between 0.04 0.9))
				   instruments))
	   (loop (- i 1)))))
  (process-remove 0)
  (let frame-loop ((i 1))
    (process i)
    (if (= i (- blocksize 1))
	(block-loop)
	(frame-loop (+ i 1)))))



