#include <stdio.h>
  
#define cel0_ValueType_Number 0
#define cel0_ValueType_Symbol 1
#define cel0_ValueType_Vector 2

typedef struct cel0_Value {
  int type;
  union {
    int number;
    char* symbol;
    struct cel0_Value* vector;
  } u;
  int size;    
} cel0_Value;

#define cel0_SymbolBindingType_Expression 0
#define cel0_SymbolBindingType_Transform 1
#define cel0_SymbolBindingType_Native 2
#define cel0_SymbolBindingType_TransformNative 3

struct cel0_SymbolBindingStack;
typedef struct cel0_SymbolBinding {
  int type;
  cel0_Value* symbol;
  union {
    cel0_Value* expression;
    cel0_Value* (*native)(cel0_Value* parameters, struct cel0_SymbolBindingStack* stack);
  } u;
} cel0_SymbolBinding;

typedef struct cel0_SymbolBindingStack {
  cel0_SymbolBinding* frames;
  int size;
  int capacity;
} cel0_SymbolBindingStack;

cel0_Value* cel0_parse(char* code);

void cel0_printValue(cel0_Value* value, FILE* fd);

cel0_Value* cel0_eval(cel0_Value* value);

