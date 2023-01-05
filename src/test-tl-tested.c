// ----------------------------------------------------------------------------

#define _GNU_SOURCE
// #define _POSIX_C_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE *_test_saved_stdin = NULL, *_test_saved_stdout = NULL, *_test_saved_stderr = NULL;

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
  _test_saved_stdin  = stdin;
  _test_saved_stdout = stdout;
  _test_saved_stderr = stderr;
}

/**
 * reset_iostreams: Restores the stdin, stdout, stderr configuration
 * that was previously saved. Should be called on test Setup or Teardown.
 */
void reset_iostreams(void) {
  if (_test_saved_stderr == NULL) {
    fprintf(stderr, "You need to initialize things by calling save_iostreams initially\n");
    exit(1);
  }
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

// /**
//  * chopped_result: Returns a portion of the result, which matches in length an expected result.
//  * The objective is to use the result of this in a string test comparison assertion.
//  * Note: Single Thread!! because it uses o static here.
//  */
// const char *chopped_result(const char *in_result, const char *expected_match) {
//   static char chopped_result[1024];
//   int copy_len = strlen(expected_match);
//   if (copy_len >= 1024) {
//     perror("expected match exceeds 1024 characters, please lower");
//     exit(1);
//   }
//   strncpy(chopped_result, in_result, copy_len);
//   chopped_result[copy_len] = '\0';
//   return chopped_result;
// }
// // ----------------------------------------------------------------------------

// Include the test framework.
#include "../test-framework/unity.h"
#include "tinylisp.h"
// Include the header file with the declarations of the functions you create.
// #include "plain.h"

// Runs before every test.
void setUp(void) {
  reset_iostreams();
  init_tinylisp();
}

// Runs after every test.
void tearDown(void) {}

/**
 *
 * NOTE: Single Thread.
 */
static const char *exercise_scan(const char *in_buf) {
  init_tinylisp();
  stdin_from_str(in_buf);
  scan();
  return buf;
}

// scan returns a single token each time.
// the possible tokens are '(', ')', '\'' and
// any string which is not composed of '(', ')' or ' '.
static void test_scan_lp(void) {
  const char *out_buf = exercise_scan("(");
  TEST_ASSERT_EQUAL_CHAR('(', *out_buf);
}

static void test_scan_del_spaces(void) {
  const char *out_buf = exercise_scan("(                )");
  TEST_ASSERT_EQUAL_CHAR('(', *out_buf);
}

static void test_scan_symbol_string(void) {
  const char expected[] = "ao%vq#";
  const char *out_buf   = exercise_scan(expected);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, out_buf, "Difference in strings");
}

static void test_scan_single_quote(void) {
  const char expected[] = "'";
  const char *out_buf   = exercise_scan(expected);
  TEST_ASSERT_EQUAL_STRING(expected, out_buf);
}

static void test_scan_quotes_in_symbol(void) {
  const char expected[] = "ao%vq\"'#";
  const char *out_buf   = exercise_scan(expected);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, out_buf, "Difference in strings");
}

typedef struct {
  const char *in;
  const char *out;
  const char *msg;
} expectation;

static void test_scanner(void) {
  // c\\lang-format off

  const expectation expectations[] = {
      {"a",                          NULL,       "letter"     },
      {"a      (",                   "a",        "letter"     },
      {"b",                          NULL,       "letter"     },
      {"(",                          NULL,       "Single LP"  },
      {"(        ",                  "(",        "Single LP"  },
      {"\"",                         NULL,       "token"      },
      {"'",                          NULL,       "msg"        },
      {"ao%vq\"'(#",                 "ao%vq\"'", "Weird token"},
      {"define",                     NULL,       "msg"        },
      {"......",                     NULL,       "msg"        },
      {"another_one_bites_the_dust", NULL,       "msg"        },
      {"________%%%__________",      NULL,       "msg"        },
      {"______'''''@#$@%^#*",        NULL,       "msg"        }
  };
  // c\\lang-format on
  for (int i = 0; i < (sizeof(expectations) / sizeof(expectation)); i++) {
    const expectation exp = expectations[i];
    char in_buf[1024];
    sprintf(in_buf, "               %s", exp.in);
    const char *out_buf = exercise_scan(in_buf);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(exp.out ? exp.out : exp.in, out_buf, exp.msg);
  }
}

#include <signal.h>

// Runs the test(s)
int main(void) {
  // Uncomment the following line to attach debugger to running program.
  // raise(SIGSTOP);

  save_iostreams();
  UnityBegin("tinylisp-tested.c");
  RUN_TEST(test_scan_lp);
  RUN_TEST(test_scan_del_spaces);
  RUN_TEST(test_scan_symbol_string);
  RUN_TEST(test_scan_single_quote);
  RUN_TEST(test_scan_quotes_in_symbol);
  RUN_TEST(test_scanner);
  // RUN_TEST(test_single_number);
  // RUN_TEST(test_simple_addition);
  // RUN_TEST(test_several_expressions);
  // RUN_TEST(test_expected);
  // RUN_TEST(test_error_empty_program);
  // RUN_TEST(test_error_invalid_character);
  // RUN_TEST(test_warning_insert_semicolon);

  return UnityEnd();
}
