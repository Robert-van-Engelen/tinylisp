
testee: str = "tinylisp-commented"

test_list = [
    {"name": "Basic addition", "send": "(+ 2 3)", "expect": "5"},
    {"name": "Basic division", "send": "(/ 6 3)", "expect": "2"},
    {
        "name": "Factorial test",
        "send": """
(define fact
  (lambda (n)
    (if (< n 2) 
      1
      (* n (fact (- n 1))))))

(fact 5)
        """,
        "expect": "120"
    },
    {
        "name": "Unimplemented operator error",
        "send": "(<: 1 2)",
        "expect": "ERR",
    }
]
