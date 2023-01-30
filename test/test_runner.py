#!/usr/bin/env python
#
# A minimally invasive test runner for tinylisp et al.
# Inspiration:Kanaka MAL tests:
#
import sys
from importlib import import_module
from pathlib import Path
import re
from subprocess import Popen, PIPE, STDOUT
from typing import List
from test_case import TestCase


regex = re.compile("\\d{1,}\\>(.*)$")


def extract_result(results: str) -> str:
    results_list = results[0].split('\n')
    result_line: str = results_list[-2]
    matchArray = regex.findall(result_line)
    return matchArray[0]


def run_test(testee: str, test_case: TestCase) -> bool:
    print(f"Testing {test_case.name}: ", end='')
    proc = Popen(["../src/" + testee], stdin=PIPE, stdout=PIPE, stderr=PIPE,
                 universal_newlines=True)
    results = proc.communicate(test_case.send + '\n')
    returned_result = extract_result(results)
    ret_val: bool = False
    if (returned_result == test_case.expect):
        ret_val = True
        print("passed")
    else:
        print("failed!")
        print(f"Expected: {test_case.expect}")
        print(f"But got: {returned_result}")
    return ret_val


def run_tests(tests) -> int:
    tests_passed: int = 0
    print(f"Running test cases on {tests.testee}:")
    for test_case in tests.test_list:
        if run_test(tests.testee, test_case):
            tests_passed += 1
    return tests_passed


def main() -> int:
    all_tests = set(_.stem for _ in Path.glob(Path("."),
                                              "*_tests.py"))
    test_choice = "tinylisp_tests"
    try:
        if sys.argv[1] in all_tests:
            test_choice = sys.argv[1]
    except IndexError:
        pass

    print(f"Importing {test_choice}")
    tests = import_module(test_choice)

    if (not Path("../src/" + tests.testee).is_file()):
        print(f"Could not locate executable {tests.testee}: Did you compile it?")
        return -1

    tests_passed: int = run_tests(tests)
    print(f"Passed {tests_passed}/{len(tests.test_list)} tests")
    return 0 if tests_passed == len(tests.test_list) else -1


if __name__ == "__main__":
    sys.exit(main())
