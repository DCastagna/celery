#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "cel0.h"

#define cel0_MaxInputBuffer (1<<10)
int main(int argc, char* argv[]) {
  (void)argc; (void)argv;
  
  char input[cel0_MaxInputBuffer];
  fgets(input, cel0_MaxInputBuffer, stdin);
  assert(strlen(input) < cel0_MaxInputBuffer - 1);
  struct cel0_Value* parsed = cel0_parse(input);
  cel0_printValue(parsed, stdout);
  printf("\n");
  struct cel0_Value* evaluated = cel0_eval(parsed);    
  cel0_printValue(evaluated, stdout);
  printf("\n");
}
