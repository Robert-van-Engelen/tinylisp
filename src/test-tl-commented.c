// ----------------------------------------------------------------------------

#define _GNU_SOURCE
// #define _POSIX_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE *_test_saved_stdin, *_test_saved_stdout, *_test_saved_stderr;

/**
 * save_iostreams: Saves stdin, stdout and stderr such that
 * a test can subsequently redirect these safely.
 * Note: Call it ONCE only on test suite setup.
 * TODO: TEST this.
 */
void save_iostreams(void) {
  if (fileno(stdin) != STDIN_FILENO || fileno(stdout) != STDOUT_FILENO ||
      fileno(stderr) != STDERR_FILENO) {
    perror("Nope, can't save custom iostreams");
    exit(1);
  }
  _test_saved_stdin = stdin;
  _test_saved_stdout = stdout;
  _test_saved_stderr = stderr;
}

/**
 * reset_iostreams: Restores the stdin, stdout, stderr configuration
 * that was previously saved. Should be called on test Setup or Teardown.
 */
void reset_iostreams(void) {
  if (stdin != _test_saved_stdin) {
    fclose(stdin);
    stdin = _test_saved_stdin;
  }
  if (stdout != _test_saved_stdout) {
    fflush(stdout);
    fclose(stdout);
    stdout = _test_saved_stdout;
  }
  if (stderr != _test_saved_stderr) {
    fflush(stderr);
    fclose(stderr);
    stderr = _test_saved_stderr;
  }
}

/**
 * stdin_from_str: Redirects stdin to read from a string.
 */
void stdin_from_str(const char *input_str) {
  stdin = fmemopen((void *)input_str, strlen(input_str), "r");
  if (stdin == NULL) {
    perror("Error creating input stream");
    exit(1);
  }
}
/**
 * stderr_to_str: Redirect the stderr stream to a string.
 * Note: Make sure the string has sufficient space.
 */
void stderr_to_str(char *buf, int buf_len) {
  stderr = fmemopen(buf, buf_len, "w");
  if (stderr == NULL) {
    perror("Error creating error stream");
    exit(1);
  }
}

/**
 * chopped_result: Returns a portion of the result, which matches in length an expected result.
 * The objective is to use the result of this in a string test comparison assertion.
 * Note: Single Thread!! because it uses o static here.
 */
const char *chopped_result(const char *in_result, const char *expected_match) {
  static char chopped_result[1024];
  int copy_len = strlen(expected_match);
  if (copy_len >= 1024) {
    perror("expected match exceeds 1024 characters, please lower");
    exit(1);
  }
  strncpy(chopped_result, in_result, copy_len);
  chopped_result[copy_len] = '\0';
  return chopped_result;
}
// ----------------------------------------------------------------------------

// Include the test framework.
#include "../test-framework/unity.h"

// Include the header file with the declarations of the functions you create.
// #include "plain.h"

// Runs before every test.
void setUp(void) {
  reset_iostreams();
  // init_lex();
}

// Runs after every test.
void tearDown(void) {}

/**
 *
 * NOTE: Single Thread.
 */
// static const char *test_statements(const char *in_buf) {
//   static char out_buf[1024];
//   stdin_from_str(in_buf);
//   out_buf[0] = '\0';
//   stderr_to_str(out_buf, 1024);
//   init_lex();
//   statements();
//   reset_iostreams();
//   return out_buf;
// }

// static void test_single_number(void) {
//   const char *result = test_statements("2;");
//   TEST_ASSERT_EMPTY_MESSAGE(result, result);
// }

// static void test_simple_addition(void) {
//   const char *result = test_statements("2 + 3;");
//   TEST_ASSERT_EMPTY_MESSAGE(result, result);
// }

// static void test_several_expressions(void) {
//   const char *input_expressions =
//       "3 * ( 4 + 3 + 2);\n"
//       "4 + 2;";
//   const char *result = test_statements(input_expressions);
//   TEST_ASSERT_EMPTY_MESSAGE(result, result);
// }
// static void test_expected(void) {
//   const char *strs[] = {"2 + 3;", "(2 + 3);"};
//   for (int i = 0; i < (sizeof(strs) / sizeof(char *)); i++) {
//     const char *result = test_statements(strs[i]);
//     TEST_ASSERT_EMPTY_MESSAGE(result, strs[i]);
//   }
// }

// static void test_warning_insert_semicolon(void) {
//   const char *result = test_statements("1");
//   const char *expected = "1: Inserting missing semicolon";
//   // I use a limited strncmp because there are more messages.
//   // Testing the first one is good enough for precision. In fact
//   // just testing that the length of expected is > 0 would
//   // suffice to establish the error condition.
//   TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(expected, result, strlen(expected)), result);
// }

// static void test_error_empty_program(void) {
//   const char *result = test_statements("");
//   const char *expected = "0: Number or identifier expected";
//   // I use a limited strncmp because there are more messages.
//   // Testing the first one is good enough for precision. In fact
//   // just testing that the length of expected is > 0 would
//   // suffice to establish the error condition.
//   TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(expected, result, strlen(expected)), result);
// }

// static void test_error_invalid_character(void) {
//   const char *expected = "Ignoring illegal input <->\n";
//   const char *result = chopped_result(test_statements("-"), expected);
//   // TEST_ASSERT_EQUAL_MESSAGE(0, strncmp(expected,  result, strlen(expected)), result);
//   TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, result, result);
// }




#include <signal.h>

// Runs the test(s)
int main(void) {
  // Uncomment the following line to attach debugger to running program.
  // raise(SIGSTOP);

  // save_iostreams();
  UnityBegin("tinylisp-commented.c");
  // RUN_TEST(test_single_number);
  // RUN_TEST(test_simple_addition);
  // RUN_TEST(test_several_expressions);
  // RUN_TEST(test_expected);
  // RUN_TEST(test_error_empty_program);
  // RUN_TEST(test_error_invalid_character);
  // RUN_TEST(test_warning_insert_semicolon);

  return UnityEnd();
}
