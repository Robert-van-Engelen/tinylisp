// Include the test framework.
#define UNITY_INCLUDE_DOUBLE
#include "../test-framework/unity.h"
#include "test-util.h"
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

static void test_read_number(void) {
  stdin_from_str("99.3\n");
  L result = Read();
  TEST_ASSERT_DOUBLE_IS_NOT_NAN(result);
  TEST_ASSERT_EQUAL_DOUBLE(99.3, result);
}

static void test_read_atom(void) {
  stdin_from_str("an_atom_here\n");
  L result = Read();
  TEST_ASSERT_DOUBLE_IS_NAN(result);
  TEST_ASSERT(T(result) == ATOM);
  TEST_ASSERT_EQUAL_STRING_MESSAGE("an_atom_here", A + ord(result), "Atom interning");
}

static void test_read_quote(void) {
  stdin_from_str("'an_quote_here\n");
  L result = Read();
  TEST_ASSERT_DOUBLE_IS_NAN(result);
  TEST_ASSERT(T(result) == CONS);
  // TEST_ASSERT_EQUAL_STRING_MESSAGE("a_quote_here", A + ord(result), "Atom interning"); 
}

#include <signal.h>

int main(void) {
  // Uncomment the following line to attach debugger to running program.
  // raise(SIGSTOP);

  save_iostreams();
  UNITY_BEGIN();
  RUN_TEST(test_scanner);
  RUN_TEST(test_read_number);
  RUN_TEST(test_read_atom);
  RUN_TEST(test_read_quote);

  return UNITY_END();
}
