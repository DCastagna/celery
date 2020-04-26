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

#define cel0_SymbolBindingType_Expression 0
#define cel0_SymbolBindingType_Transform 1
#define cel0_SymbolBindingType_Native 2
#define cel0_SymbolBindingType_TransformNative 3

struct cel0_SymbolBindingStack;
struct cel0_SymbolBinding {
  int type;
  struct cel0_Value* symbol;
  union {
    struct cel0_Value* expression;
    struct cel0_Value* (*native)(struct cel0_Value* parameters, struct cel0_SymbolBindingStack* stack);
  } u;
};
struct cel0_SymbolBindingStack {
  struct cel0_SymbolBinding* frames;
  int size;
  int capacity;
};

struct cel0_Value* cel0_parse(char* code);

void cel0_printValue(struct cel0_Value* value, FILE* fd);

struct cel0_Value* cel0_eval(struct cel0_Value* value);

