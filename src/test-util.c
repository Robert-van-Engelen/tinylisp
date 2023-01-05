#define _GNU_SOURCE
// #define _POSIX_C_SOURCE

#include "test-util.h"

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
// // ----