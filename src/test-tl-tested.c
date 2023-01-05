// ----------------------------------------------------------------------------

#define _GNU_SOURCE
// #define _POSIX_C_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------------------

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
typedef struct {
  const char *in;
  const char *out;
  const char *msg;
} expectation;

static void test_scanner(void) {
  const expectation expectations[] = {
      {"a",                          NULL,       "letter"                      },
      {"a      (",                   "a",        "letter"                      },
      {"b",                          NULL,       "letter"                      },
      {"(",                          NULL,       "Single LP"                   },
      {"(        )",                 "(",        "Single LP followed by spaces"},
      {"\"",                         NULL,       "token"                       },
      {"'",                          NULL,       "Single quote"                },
      {"ao%vq\"'(#",                 "ao%vq\"'", "Token finished by LP '('"    },
      {"define",                     NULL,       "define simple token"         },
      {"......",                     NULL,       "dots are a token too"        },
      {"another_one_bites_the_dust", NULL,       "a long feasible token"       },
      {"________%%%__________",      NULL,       "One more odd token"          },
      {"______'''''@#$@%^#*",        NULL,       "Another, odd but valid token"}
  };

  for (int i = 0; i < (sizeof(expectations) / sizeof(expectation)); i++) {
    const expectation exp = expectations[i];
    char in_buf[1024];
    sprintf(in_buf, "               %s", exp.in);
    const char *out_buf = exercise_scan(in_buf);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(exp.out ? exp.out : exp.in, out_buf, exp.msg);
  }
}

#include <signal.h>

int main(void) {
  // Uncomment the following line to attach debugger to running program.
  // raise(SIGSTOP);

  save_iostreams();
  UnityBegin("tinylisp-tested.c");
  RUN_TEST(test_scanner);

  return UnityEnd();
}
