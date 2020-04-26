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
    fprintf(fd, ")");        
  } else if (value->type == cel0_ValueType_Number) {
    fprintf(fd, "%d ", value->u.number);
  } else if (value->type == cel0_ValueType_Symbol) {
    fprintf(fd, "%s ", value->u.symbol);    
  }
}
