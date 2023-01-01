#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  char *input;

  using_history();
  while (1) {
    input = readline("Enter a line of input: ");
    printf("You entered: %s\n", input);
    add_history(input);
    free(input);
  }

  return 0;
}
