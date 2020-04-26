#include <assert.h>
#include <stdio.h>

#include "cel0.h"

int main(int argc, char* argv[]) {
  assert(argc > 1);
  struct cel0_Value* parsed = cel0_parse(argv[1]);
  cel0_printValue(parsed, stdout);
  printf("\n");

  struct cel0_Value* evaluated = cel0_eval(parsed);    
  cel0_printValue(evaluated, stdout);
  printf("\n");

}
