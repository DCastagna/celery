#include <stdio.h>

#define cel0_ValueType_Number 0
#define cel0_ValueType_Symbol 1
#define cel0_ValueType_Vector 2

struct cel0_Value {
  int type;
  union {
    int number;
    char* symbol;
    struct cel0_Value* vector;
  } u;
  int size;    
};

struct cel0_Value* cel0_parse(char* code);
void cel0_printValue(struct cel0_Value* value, FILE* fd);
