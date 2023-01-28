from typing import List
from test_case import TestCase

testee : str = "tinylisp-commented"

test_list : List[TestCase] = [
    TestCase(name = "Basic addition", send = "(+ 2 3)", expect = "5"),
    TestCase(name = "Basic division", send = "(/ 6 3)", expect = "2"),
    TestCase(
        name = "Factorial test",
        send = """
(define fact
  (lambda (n)
    (if (< n 2) 
      1
      (* n (fact (- n 1))))))

(fact 5)
        """,
        expect = "120"
    ),
    TestCase(
      name = "Unimplemented operator error",
      send = "(<= 1 2)",
      expect = "ERR",
    )    
]
