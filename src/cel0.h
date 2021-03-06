#include <assert.h>
#include <stdio.h>
  
#define cel0_ValueType_Number 0
#define cel0_ValueType_Symbol 1
#define cel0_ValueType_Vector 2
#define cel0_ValueType_Panic 3

typedef struct cel0_Value {
  int type;
  union {
    int number;
    int symbol_id;
    int vector_id;    
  } u;
} cel0_Value;
static_assert(sizeof(cel0_Value) == 8, "cel0_Value should be 64 bits");

#define cel0_SymbolBindingType_Expression 0
#define cel0_SymbolBindingType_Native 1
#define cel0_SymbolBindingType_TransformNative 2

struct cel0_SymbolBindingStack;
typedef struct cel0_SymbolBinding {
  int type;
  cel0_Value* symbol;
  union {
    cel0_Value* expression;
    struct {
      cel0_Value* (*expression)(cel0_Value* parameters, struct cel0_SymbolBindingStack* stack);
      cel0_Value* (*captureLexicalBindings)(cel0_Value* parameters, struct cel0_SymbolBindingStack* stack);
    } native;
  } u;
} cel0_SymbolBinding;

typedef struct cel0_SymbolBindingStack {
  struct cel0_SymbolBindingStack* previous;
  cel0_SymbolBinding* frames;
  int global_size;
  int begin;
  int size;
  int capacity;
} cel0_SymbolBindingStack;

cel0_Value* cel0_parse(char* code);

void cel0_printValue(cel0_Value* value, FILE* fd);

cel0_Value* cel0_eval(cel0_Value* value);
