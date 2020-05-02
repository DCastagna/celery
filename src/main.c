#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cel0.h"

int main(int argc, char* argv[]) {
  (void)argc; (void)argv;

  int input_size = 1024;
  char* input = malloc(1024);
  assert(input);
  int chars_read = 0;
  do {
    fgets(input + chars_read, input_size - chars_read, stdin);
    chars_read += strlen(input + chars_read);
    if (chars_read == input_size - 1) {
      input_size *= 2;
      input = realloc(input, input_size);
      assert(input);
    }
  } while(!feof(stdin));
  
  struct cel0_Value* parsed = cel0_parse(input);
  struct cel0_Value* evaluated = cel0_eval(parsed);    
  cel0_printValue(evaluated, stdout);
  printf("\n");
}
