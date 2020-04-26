#include "cel0.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cel0_Value* createNumberValue(int number) {
  struct cel0_Value* value = malloc(sizeof(struct cel0_Value));
  value->type = cel0_ValueType_Number;
  value->u.number = number;
  return value;
}

#define cel0_MaxSymbolLength 16
#define cel0_MaxSymbols (1<<10)

static char g_symbols[cel0_MaxSymbolLength][cel0_MaxSymbols];
static int g_symbol_number = 0;
static char* internSymbol(char* name) {
  assert(strlen(name) < cel0_MaxSymbolLength);  
  for (int i=0; i<g_symbol_number; i++) {
    if (strcmp(g_symbols[i], name) == 0)
      return g_symbols[i];
  }
  assert(g_symbol_number < cel0_MaxSymbols);

  strcpy(g_symbols[g_symbol_number], name);
  return g_symbols[g_symbol_number++];
}

static struct cel0_Value* createSymbolValue(char* name) {
  assert(name);
  
  struct cel0_Value* value = malloc(sizeof(struct cel0_Value));
  value->type = cel0_ValueType_Symbol;
  value->u.symbol = internSymbol(name);
  return value;
}
    
static struct cel0_Value* createVectorValue() {
  struct cel0_Value* value = malloc(sizeof(struct cel0_Value));
  value->u.vector = malloc(0);
  value->type = cel0_ValueType_Vector;
  value->size = 0;
  return value;
}
static struct cel0_Value* appendValueToVectorInPlace(struct cel0_Value* vector, struct cel0_Value* value) {
  assert(vector);
  assert(vector->type == cel0_ValueType_Vector);
  assert(value);
  
  int new_size = vector->size + 1;
  vector->u.vector = realloc(vector->u.vector, new_size * sizeof(struct cel0_Value));
  vector->u.vector[vector->size] = *value;
  vector->size = new_size;
  return vector;
}

static struct cel0_Value* parseNumber(char** code) {
  int result = 0;
  char* code_it = *code;
  char negative = *code_it == '-';
  if (negative) code_it++;
  while (*code_it >= '0' && *code_it <= '9') {
    result = result * 10 + (*code_it - '0');
    code_it++;
  }
  assert(strspn(code_it, " \f\n\r\t\v)") || *code_it == 0);
  if (negative) result = -result;
  *code = code_it;
  return createNumberValue(result);
}

static struct cel0_Value* parseSymbol(char** code) {
  char end[] = " \f\n\r\t\v)(\0";
  
  int length = strcspn(*code, end);
  assert(length < cel0_MaxSymbolLength);
  char name[cel0_MaxSymbolLength];
  strncpy(name, *code, length);
  name[length] = 0;
  *code += length;
  return createSymbolValue(name);
}

static struct cel0_Value* parse(char** code) {
  char whitespaces[] = " \f\n\r\t\v";  
  char* code_it = *code + strspn(*code, whitespaces);
  struct cel0_Value* value = 0;

  if (*code_it == '(') {
    code_it++;
    value = createVectorValue();
    while (*code_it != ')' && *code_it != 0) {
      value = appendValueToVectorInPlace(value, parse(&code_it));
      code_it = code_it + strspn(code_it, whitespaces);
    }
    assert(*code_it);
    code_it++;
  } else if (*code_it == '-' || (*code_it >= '0' && *code_it <= '9')) {
    value = parseNumber(&code_it);
  } else if (*code_it != 0) {
    value = parseSymbol(&code_it);
  }
  *code = code_it;
  return value;
}

struct cel0_Value* cel0_parse(char* code) {
  return parse(&code);
}

void cel0_printValue(struct cel0_Value* value, FILE* fd) {
  if (value->type == cel0_ValueType_Vector) {
    fprintf(fd, "(");    
    for (int i=0; i<value->size; i++) {
      cel0_printValue(value->u.vector + i, fd);
    }
    fprintf(fd, "\b) ");        
  } else if (value->type == cel0_ValueType_Number) {
    fprintf(fd, "%d ", value->u.number);
  } else if (value->type == cel0_ValueType_Symbol) {
    fprintf(fd, "%s ", value->u.symbol);    
  }
}

struct cel0_SymbolBinding* lookupSymbolBinding(struct cel0_SymbolBindingStack* stack, struct cel0_Value* key) {
  assert(key->type == cel0_ValueType_Symbol);  
  for (int i=stack->size-1; i>=0; i-=1) {
    struct cel0_SymbolBinding* binding = stack->frames + i;
    assert(binding->symbol->type == cel0_ValueType_Symbol);
    if (binding->symbol->u.symbol == key->u.symbol) {
      return binding;
    }
  }
  assert(0 && "We couldn't find the binding");
  return 0;
}

static struct cel0_Value* eval(struct cel0_Value* value, struct cel0_SymbolBindingStack* stack) {
  if (value->type == cel0_ValueType_Vector) {
    int caller_stack_size = stack->size;
    assert(value->size > 0);
    struct cel0_SymbolBinding* binding = lookupSymbolBinding(stack, value->u.vector);
    assert(binding && "can't find binding.");
    struct cel0_Value* parameters = createVectorValue();
    char eval_parameters = binding->type != cel0_SymbolBindingType_TransformNative;
    for (int i=1; i<value->size; i++) {
      struct cel0_Value* p = eval_parameters ? eval(value->u.vector + i, stack) : (value->u.vector + i);
      parameters = appendValueToVectorInPlace(parameters, p);
    }

    if (binding->type == cel0_SymbolBindingType_TransformNative ||
	binding->type == cel0_SymbolBindingType_Native) {
       return binding->u.native(parameters, stack);
    } else {
      assert(binding->type == cel0_SymbolBindingType_Expression);
      assert("lambda/transform  not implemented yet.");	
    }
    stack->size = caller_stack_size;
  } else if (value->type == cel0_ValueType_Number) {
    return value;
  } else if (value->type == cel0_ValueType_Symbol) {
    struct cel0_SymbolBinding* binding = lookupSymbolBinding(stack, value);
    assert(binding && "can't find binding.");
    if (binding->type == cel0_SymbolBindingType_Expression) {
      return binding->u.expression;
    } else if (binding->type == cel0_SymbolBindingType_Native ||
	       binding->type == cel0_SymbolBindingType_TransformNative ){
      char name[cel0_MaxSymbolLength];
      sprintf(name,cel0_SymbolBindingType_Native ? "#native-%p" : "#t-native-%p", (void*)(binding->u.expression));
      return createSymbolValue(name);
    }
  }
  assert(0);
}

struct cel0_Value* bind(struct cel0_Value* params, struct cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  assert(params->size == 2);
  assert(params->u.vector[0].type == cel0_ValueType_Vector);
  assert(params->u.vector[0].size == 2);
  assert(params->u.vector[0].u.vector[0].type == cel0_ValueType_Symbol);    

  assert(stack->size < stack->capacity);

  struct cel0_Value* bound_expression = eval(params->u.vector[0].u.vector + 1, stack);
  
  struct cel0_SymbolBinding* binding = stack->frames + stack->size;
  binding->type = cel0_SymbolBindingType_Expression;
  binding->symbol = params->u.vector[0].u.vector;
  binding->u.expression = bound_expression;
  int caller_stack_size = stack->size;
  stack->size++;
  struct cel0_Value* result = eval(params->u.vector + 1, stack);
  stack->size = caller_stack_size;
  return result;
}

struct cel0_Value* add(struct cel0_Value* params, struct cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  int result = 0;
  for (int i=0; i<params->size; i++) {
    assert(params->u.vector[i].type == cel0_ValueType_Number);
    result += params->u.vector[i].u.number;
  }
  return createNumberValue(result);
}


struct cel0_Value* mul(struct cel0_Value* params, struct cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  int result = 1;
  for (int i=0; i<params->size; i++) {
    assert(params->u.vector[i].type == cel0_ValueType_Number);
    result *= params->u.vector[i].u.number;
  }
  return createNumberValue(result);
}

#define cel0_SymbolBindingFrameCapacity 1<<10

struct cel0_Value* cel0_eval(struct cel0_Value* value) {
  // TODO: add #mul, #add, #bind & co

  int capacity = cel0_SymbolBindingFrameCapacity;
  struct cel0_SymbolBinding frames[capacity];

  int size = 0;
  frames[size].type = cel0_SymbolBindingType_Native;
  frames[size].symbol = createSymbolValue("add");
  frames[size].u.native = add;
  size++;
  frames[size].type = cel0_SymbolBindingType_Native;
  frames[size].symbol = createSymbolValue("mul");
  frames[size].u.native = mul;
  size++;
  frames[size].type = cel0_SymbolBindingType_TransformNative;
  frames[size].symbol = createSymbolValue("bind");
  frames[size].u.native = bind;
  size++;
  
  struct cel0_SymbolBindingStack stack = {.frames = frames, .size = size, .capacity = capacity };

  
  stack.frames = frames;
  return eval(value, &stack);
}
