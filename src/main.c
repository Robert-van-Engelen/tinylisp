#include <stdio.h>

#include "tinylisp.h"

// #include <signal.h>

int main() {
  // raise(SIGSTOP);
  printf("tinylisp");
  init();
  while (1) {
    printf("\n%u>", sp - hp / 8);
    print(eval(read(), env));
    gc();
  }
}
