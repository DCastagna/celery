#include "cel0.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cel0_Value* createNumberValue(int number) {
  cel0_Value* value = malloc(sizeof(cel0_Value));
  assert(value);
  value->type = cel0_ValueType_Number;
  value->u.number = number;
  return value;
}

#define cel0_MaxSymbolLength 32
#define cel0_MaxSymbols (1<<10)

static char g_symbols[cel0_MaxSymbols][cel0_MaxSymbolLength];
static int g_symbol_number = 0;

static int internSymbol(char* name) {
  assert(strlen(name) < cel0_MaxSymbolLength);
  for (int i=0; i<g_symbol_number; i++) {
    if (strcmp(g_symbols[i], name) == 0)
      return i;
  }
  assert(g_symbol_number < cel0_MaxSymbols && "Reached maximum number of symbols.");

  strcpy(g_symbols[g_symbol_number], name);
  return g_symbol_number++;
}

static char* lookupSymbolName(int symbol_id) {
  assert(symbol_id < g_symbol_number);
  return g_symbols[symbol_id];
}

static cel0_Value* createPanicValue(char* symbol);
static cel0_Value* createSymbolValue(char* name) {
  assert(name);
  cel0_Value* value = malloc(sizeof(cel0_Value));
  assert(value);
  value->type = cel0_ValueType_Symbol;
  value->u.symbol_id = internSymbol(name);
  return value;
}

#define cel0_MaxVectorMetadataLength (1<<20)
typedef struct cel0_VectorMetadata {
  cel0_Value* vector;
  int size;
} cel0_VectorMetadata;
cel0_VectorMetadata g_vector_metadata[cel0_MaxVectorMetadataLength];

static int g_vector_metadata_number = 0;
static int createVectorMetadata() {
  assert(g_vector_metadata_number < cel0_MaxVectorMetadataLength && "Reached maximum number of values.");
  memset(g_vector_metadata + g_vector_metadata_number, 0, sizeof(cel0_VectorMetadata));
  return g_vector_metadata_number++;
}

static cel0_VectorMetadata* lookupVectorMetadata(int vector_id) {
  assert(vector_id < g_vector_metadata_number);
  return g_vector_metadata + vector_id;
}

static cel0_Value* createVectorValue() {
  cel0_Value* value = malloc(sizeof(cel0_Value));
  assert(value);
  value->type = cel0_ValueType_Vector;  
  value->u.vector_id = createVectorMetadata();
  cel0_VectorMetadata* metadata = lookupVectorMetadata(value->u.vector_id);
  metadata->vector = malloc(0);
  metadata->size = 0;
  return value;
}

static cel0_Value* appendValueToVectorInPlace(cel0_Value* vector, cel0_Value* value) {
  assert(vector);
  assert(value);
  assert(vector->type == cel0_ValueType_Vector || vector->type == cel0_ValueType_Panic);

  cel0_VectorMetadata* metadata = lookupVectorMetadata(vector->u.vector_id);
  int new_size = metadata->size + 1;
  metadata->vector = realloc(metadata->vector, new_size * sizeof(cel0_Value));
  assert(metadata->vector);
  metadata->vector[metadata->size] = *value;
  metadata->size = new_size;
  return vector;
}

static cel0_Value* concatVectorsInPlace(cel0_Value* dest_vector, cel0_Value* append) {
  assert(dest_vector);
  assert(append);
  assert(dest_vector->type == cel0_ValueType_Vector);
  assert(append->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* dest_vector_metadata = lookupVectorMetadata(dest_vector->u.vector_id);
  cel0_VectorMetadata* append_metadata = lookupVectorMetadata(append->u.vector_id);
  int new_size = dest_vector_metadata->size + append_metadata->size;
  dest_vector_metadata->vector = realloc(dest_vector_metadata->vector, new_size * sizeof(cel0_Value));
  assert(new_size == 0 || dest_vector_metadata->vector);
  memcpy(dest_vector_metadata->vector + dest_vector_metadata->size, append_metadata->vector, append_metadata->size * sizeof(cel0_Value));
  dest_vector_metadata->size = new_size;
  return dest_vector;
}

static cel0_Value* createPanicValue(char* symbol) {
  cel0_Value* value = malloc(sizeof(cel0_Value));
  assert(value);
  value->type = cel0_ValueType_Panic;  
  value->u.vector_id = createVectorMetadata();
  cel0_VectorMetadata* metadata = lookupVectorMetadata(value->u.vector_id);
  metadata->vector = createSymbolValue(symbol);
  assert(metadata->vector && metadata->vector->type == cel0_ValueType_Symbol);
  metadata->size = 1;
  return value;
}

static cel0_Value* createPanicValueWithParam(char* symbol, cel0_Value* param) {
  assert(param);
  cel0_Value* value = malloc(sizeof(cel0_Value));
  assert(value);
  value->type = cel0_ValueType_Panic;  
  value->u.vector_id = createVectorMetadata();
  cel0_VectorMetadata* metadata = lookupVectorMetadata(value->u.vector_id);
  metadata->vector = createVectorValue();
  metadata->size = 1;  
  assert(metadata->vector);
  metadata->vector = appendValueToVectorInPlace(metadata->vector, createSymbolValue(symbol));
  metadata->vector = appendValueToVectorInPlace(metadata->vector, param);  
  return value;
}

static cel0_Value* parseNumber(char** code) {
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

static cel0_Value* parseSymbol(char** code) {
  char end[] = " \f\n\r\t\v)(\0";
  int length = strcspn(*code, end);
  char name[cel0_MaxSymbolLength];
  strncpy(name, *code, length);
  name[length] = 0;  
  if (length >= cel0_MaxSymbolLength) return createPanicValueWithParam("symbol-too-long", createSymbolValue(name));  
  *code += length;
  return createSymbolValue(name);
}

static cel0_Value* parse(char** code) {
  char whitespaces[] = " \f\n\r\t\v";  
  char* code_it = *code + strspn(*code, whitespaces);
  cel0_Value* value = 0;

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

cel0_Value* cel0_parse(char* code) {
  return parse(&code);
}

void cel0_printValue(cel0_Value* value, FILE* fd) {
  assert (value->type == cel0_ValueType_Vector ||
	  value->type == cel0_ValueType_Number ||
	  value->type == cel0_ValueType_Symbol ||
	  value->type == cel0_ValueType_Panic);
  if (value->type == cel0_ValueType_Vector || value->type == cel0_ValueType_Panic) {
    char panic = value->type == cel0_ValueType_Panic;
    fprintf(fd,  panic ? "<" : "(");
    cel0_VectorMetadata* metadata = lookupVectorMetadata(value->u.vector_id);
    for (int i=0; i<metadata->size; i++) {
      cel0_printValue(metadata->vector + i, fd);
      if (i != metadata->size - 1)
	fprintf(fd, " ");              
    }
    fprintf(fd, panic ? ">" : ")");
  } else if (value->type == cel0_ValueType_Number) {
    fprintf(fd, "%d", value->u.number);
  } else if (value->type == cel0_ValueType_Symbol) {
    fprintf(fd, "%s", lookupSymbolName(value->u.symbol_id));    
  }
}

static cel0_SymbolBinding* lookupSymbolBinding(cel0_Value* key, cel0_SymbolBindingStack* stack) {
  assert(key->type == cel0_ValueType_Symbol);
  for (int i=stack->size-1; i>=stack->begin; i-=1) {
    cel0_SymbolBinding* binding = stack->frames + i;
    assert(binding->symbol->type == cel0_ValueType_Symbol);
    if (binding->symbol->u.symbol_id == key->u.symbol_id) {
      return binding;
    }
  }
  for (int i=0; i<stack->global_size; i++) {
    cel0_SymbolBinding* binding = stack->frames + i;
    assert(binding->symbol->type == cel0_ValueType_Symbol);
    if (binding->symbol->u.symbol_id == key->u.symbol_id) {
      return binding;
    }
  }
  return 0;
}

static cel0_Value* placeHolderForRecursion() {
  static cel0_Value* place_holder_for_recursion = 0;
  if (!place_holder_for_recursion) {
    place_holder_for_recursion  = createSymbolValue("#self-rec");
  }
  return place_holder_for_recursion;
}

static cel0_Value* eval(cel0_Value* value, cel0_SymbolBindingStack* stack);
static cel0_Value* apply(cel0_Value* expression, cel0_Value* parameters, cel0_SymbolBindingStack* stack) {
  assert(expression && expression->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* expression_metadata = lookupVectorMetadata(expression->u.vector_id);
  if (expression_metadata->size != 2 ||
      expression_metadata->vector[0].type != cel0_ValueType_Vector)
    return createPanicValue("ill-formed");
  cel0_VectorMetadata* lambda_list_metadata =
    lookupVectorMetadata(expression_metadata->vector->u.vector_id);  
  assert(parameters->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* parameters_metadata = lookupVectorMetadata(parameters->u.vector_id);
  int number_param = 0;
  for (; lambda_list_metadata->vector[number_param].type == cel0_ValueType_Symbol  &&
	 number_param<lambda_list_metadata->size; number_param++);

  if (number_param != parameters_metadata->size)
    return createPanicValue("number-params");

  int caller_stack_begin = stack->begin;
  stack->begin = stack->size;
  for (int i=0; i<lambda_list_metadata->size; i++) {
    assert(stack->size < stack->capacity);
    cel0_SymbolBinding* binding = stack->frames + stack->size;
    binding->type = cel0_SymbolBindingType_Expression;
    if (i<number_param) {
      binding->symbol = lambda_list_metadata->vector + i;
      binding->u.expression = parameters_metadata->vector + i;
    } else {
      assert(lambda_list_metadata->vector[i].type == cel0_ValueType_Vector);
      cel0_VectorMetadata* param_metadata =
	lookupVectorMetadata(lambda_list_metadata->vector[i].u.vector_id);
      assert(param_metadata->size == 2);
      assert(param_metadata->vector->type == cel0_ValueType_Symbol);
      binding->symbol = param_metadata->vector;
      binding->u.expression = param_metadata->vector + 1;
    }
    stack->size++;
  }
  assert(stack->size < stack->capacity);
  cel0_SymbolBinding* binding = stack->frames + stack->size;
  binding->type = cel0_SymbolBindingType_Expression;
  binding->symbol = placeHolderForRecursion();
  binding->u.expression = expression;
  stack->size++;
  cel0_Value* result = eval(expression_metadata->vector + 1, stack);
  stack->size = stack->begin;
  stack->begin = caller_stack_begin;
  return result;
}

static cel0_Value* createEvalPanicStackEntry(cel0_Value* symbol, cel0_Value* parameters) {
  cel0_Value* stack_entry = appendValueToVectorInPlace(createVectorValue(), symbol);
  assert(parameters->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* metadata = lookupVectorMetadata(parameters->u.vector_id);
  for (int i=0; i<metadata->size; i++) {
    stack_entry = appendValueToVectorInPlace(stack_entry, metadata->vector + i);
  }
  return stack_entry;
}

static cel0_Value* eval(cel0_Value* value, cel0_SymbolBindingStack* stack) {
  assert(value);
  assert(stack);

  if (value->type == cel0_ValueType_Vector) {
    cel0_VectorMetadata* value_metadata = lookupVectorMetadata(value->u.vector_id);
    if (value_metadata->size == 0) return createPanicValue("empty-vec");

    assert(value_metadata->size > 0);
    if (value_metadata->vector->type != cel0_ValueType_Symbol) return createPanicValue("not-symbol");
    
    cel0_SymbolBinding* binding = lookupSymbolBinding(value_metadata->vector, stack);
    if(!binding) return createPanicValueWithParam("unbound", value_metadata->vector);
    
    cel0_Value* parameters = createVectorValue();
    char eval_parameters =
      binding->type == cel0_SymbolBindingType_Native ||
      binding->type == cel0_SymbolBindingType_Expression;
    for (int i=1; i<value_metadata->size; i++) {
      cel0_Value* p = eval_parameters ?
	eval(value_metadata->vector + i, stack) :
	(value_metadata->vector + i);
      if (p->type == cel0_ValueType_Panic) return p;

      parameters = appendValueToVectorInPlace(parameters, p);
    }
    if (binding->type == cel0_SymbolBindingType_TransformNative ||
	binding->type == cel0_SymbolBindingType_Native) {
      cel0_Value* ret =  binding->u.native.expression(parameters, stack);
      if (ret->type == cel0_ValueType_Panic)
	return appendValueToVectorInPlace(ret, createEvalPanicStackEntry(binding->symbol, parameters));
      return ret;
    } else {
      assert(binding->type == cel0_SymbolBindingType_Expression);
      cel0_Value* expression = binding->u.expression;
      if (expression->type == cel0_ValueType_Symbol && expression->u.symbol_id == placeHolderForRecursion()->u.symbol_id) {
	cel0_SymbolBinding* self_rec_binding = lookupSymbolBinding(expression, stack);
	if (!self_rec_binding) return createPanicValue("self-rec-binding");
	assert(self_rec_binding->type == cel0_SymbolBindingType_Expression);
	expression = self_rec_binding->u.expression;
      }
      cel0_Value* ret = apply(expression, parameters, stack);
      if (ret->type == cel0_ValueType_Panic)
	return appendValueToVectorInPlace(ret, createEvalPanicStackEntry(binding->symbol, parameters));
      return ret;
    }
  } else if (value->type == cel0_ValueType_Number) {
    return value;
  } else if (value->type == cel0_ValueType_Symbol) {
    cel0_SymbolBinding* binding = lookupSymbolBinding(value, stack);
    if (!binding) return createPanicValueWithParam("unbound", value);
    if (binding->type == cel0_SymbolBindingType_Expression) {
      return binding->u.expression;
    } else if (binding->type == cel0_SymbolBindingType_Native ||
	       binding->type == cel0_SymbolBindingType_TransformNative ){
      char name[cel0_MaxSymbolLength];
      sprintf(name, binding->type == cel0_SymbolBindingType_Native ? "@%p" : "#%p", (void*)(binding->u.expression));
      return createSymbolValue(name);
    }
  }
  assert(0);
}

static cel0_Value* bind(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);
  if (params_metadata->size < 2) return createPanicValueWithParam("ill-formed", params);
  int caller_stack_size = stack->size;
  for (int i=0; i<params_metadata->size-1; i+=1) {
    if (params_metadata->vector[i].type != cel0_ValueType_Vector) return createPanicValueWithParam("ill-formed", params);
    cel0_VectorMetadata* binding_metadata =
      lookupVectorMetadata(params_metadata->vector[i].u.vector_id);    

    if (binding_metadata->size != 2 || binding_metadata->vector->type != cel0_ValueType_Symbol)
      return createPanicValue("ill-formed");

    assert(stack->size < stack->capacity);
    cel0_SymbolBinding* binding = stack->frames + stack->size;    
    binding->type = cel0_SymbolBindingType_Expression;
    binding->symbol = binding_metadata->vector;
    binding->u.expression = placeHolderForRecursion();
    stack->size++;
    cel0_Value* bound_expression = eval(binding_metadata->vector + 1, stack);
    if (bound_expression->type == cel0_ValueType_Panic) return bound_expression;    
    binding->u.expression = bound_expression;    
  }

  cel0_Value* result = eval(params_metadata->vector + params_metadata->size - 1, stack);
  stack->size = caller_stack_size;
  return result;
}

static cel0_Value* captureLexicalBindings(cel0_Value* expression, cel0_SymbolBindingStack* stack) {
  assert(expression);
  if (expression->type == cel0_ValueType_Symbol) {
    cel0_SymbolBinding* binding = lookupSymbolBinding(expression, stack);
    if (!binding) return createPanicValueWithParam("unbound", expression);
    if (binding->type != cel0_SymbolBindingType_Expression) return createVectorValue();
    if (binding->symbol == binding->u.expression) return createVectorValue();
    cel0_Value* entry = appendValueToVectorInPlace(createVectorValue(), binding->symbol);
    entry = appendValueToVectorInPlace(entry, binding->u.expression);
    return appendValueToVectorInPlace(createVectorValue(), entry);
  } else if (expression->type == cel0_ValueType_Vector) {
    cel0_VectorMetadata* expression_metadata = lookupVectorMetadata(expression->u.vector_id);
    if (expression_metadata->size == 0) return createPanicValue("empty-vec");
    if (expression_metadata->vector->type != cel0_ValueType_Symbol) createPanicValue("not-symbol");
    cel0_SymbolBinding* binding = lookupSymbolBinding(expression_metadata->vector, stack);
    if (!binding) return createPanicValueWithParam("unbound", expression_metadata->vector);
    if (binding->type == cel0_SymbolBindingType_TransformNative) {
      assert(binding->u.native.captureLexicalBindings);
      cel0_Value* params = createVectorValue();
      for (int i=1; i<expression_metadata->size; i++)
	params = appendValueToVectorInPlace(params, expression_metadata->vector + i);
      return binding->u.native.captureLexicalBindings(params, stack);
    } else {
      cel0_Value* result = createVectorValue();      
      for (int i=0; i<expression_metadata->size; i++) {
	cel0_Value* param_result = captureLexicalBindings(expression_metadata->vector + i, stack);
	if (param_result->type == cel0_ValueType_Panic) return param_result;	
	result = concatVectorsInPlace(result, param_result);	  
      }
      return result;
    }
  } else if (expression->type == cel0_ValueType_Number) {
    return createVectorValue();
  }
  assert(0);
}

static cel0_Value* bindCaptureLexicalBindings(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);
  if (params_metadata->size < 2) return createPanicValueWithParam("ill-formed", params);

  cel0_Value* result = createVectorValue();
  int caller_stack_size = stack->size;
  for (int i=0; i<params_metadata->size-1; i++) {
    if (params_metadata->vector[i].type != cel0_ValueType_Vector) return createPanicValueWithParam("ill-formed", params);
    cel0_VectorMetadata* binding_metadata =
      lookupVectorMetadata(params_metadata->vector[i].u.vector_id);    

    if (binding_metadata->size != 2 || binding_metadata->vector->type != cel0_ValueType_Symbol)
      return createPanicValue("ill-formed");

    assert(stack->size < stack->capacity);
    cel0_SymbolBinding* binding = stack->frames + stack->size;
    binding->type = cel0_SymbolBindingType_Expression;
    binding->symbol = binding_metadata->vector;
    binding->u.expression = placeHolderForRecursion();
    stack->size++;    
    cel0_Value* bindings = captureLexicalBindings(binding_metadata->vector + 1, stack);
    if (bindings->type == cel0_ValueType_Panic) return bindings;
    binding->u.expression = binding_metadata->vector;
    result = concatVectorsInPlace(result, bindings);

  }
  cel0_Value* body_bindings = captureLexicalBindings(params_metadata->vector + params_metadata->size - 1,stack);
  if (body_bindings->type == cel0_ValueType_Panic) return body_bindings;  
  result = concatVectorsInPlace(result, body_bindings);
  stack->size = caller_stack_size;
  return result;
}

static cel0_Value* ifCaptureLexicalBindings(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);    
  if(params_metadata->size != 3) return createPanicValue("ill-formed");

  cel0_Value* result = createVectorValue();
  for (int i=0; i<3; i++) {
    cel0_Value* bindings = captureLexicalBindings(params_metadata->vector + i,stack);    
    if (bindings->type == cel0_ValueType_Panic) return bindings;  
    result = concatVectorsInPlace(result, bindings);
  }
  return result;
}

static cel0_Value* quoteCaptureLexicalBindings(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  (void)params;
  (void)stack;
  return createVectorValue();
}

static cel0_Value* lambdaCaptureLexicalBindings(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);  
  assert(stack);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);  
  if (params_metadata->size != 2 || 
      params_metadata->vector[0].type != cel0_ValueType_Vector) {
    return createPanicValue("ill-formed");
  }
  cel0_VectorMetadata* lambda_list_metadata = lookupVectorMetadata(params_metadata->vector->u.vector_id);    
  for (int i=0; i<lambda_list_metadata->size; i++) {
    if (lambda_list_metadata->vector[i].type != cel0_ValueType_Symbol)
      return createPanicValueWithParam("lambda-list-ill-formed", lambda_list_metadata->vector + i);
  }
  int caller_stack_size = stack->size;
  for (int i=0; i<lambda_list_metadata->size; i++) {
    assert(stack->size < stack->capacity);    
    cel0_SymbolBinding* binding = stack->frames + stack->size;
    binding->type = cel0_SymbolBindingType_Expression;
    binding->symbol = lambda_list_metadata->vector + i;
    binding->u.expression = lambda_list_metadata->vector + i;
    stack->size++;
  }

  cel0_Value* body_bindings = captureLexicalBindings(params_metadata->vector + 1, stack);
  stack->size = caller_stack_size;
  
  return body_bindings;
}

static cel0_Value* lambda(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(stack);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* metadata = lookupVectorMetadata(params->u.vector_id);  
  if (metadata->size != 2 || 
      metadata->vector[0].type != cel0_ValueType_Vector) {
    return createPanicValue("ill-formed");
  }

  cel0_VectorMetadata* lambda_list_metadata = lookupVectorMetadata(metadata->vector->u.vector_id);    
  for (int i=0; i<lambda_list_metadata->size; i++) {
    if (lambda_list_metadata->vector[i].type != cel0_ValueType_Symbol)
      return createPanicValueWithParam("lambda-list-ill-formed", lambda_list_metadata->vector + i);
  }
  cel0_Value* params_with_capture = createVectorValue();
  params_with_capture = concatVectorsInPlace(params_with_capture, metadata->vector);
  cel0_Value* captures = lambdaCaptureLexicalBindings(params, stack);
  if (captures->type == cel0_ValueType_Panic) return captures;
  params_with_capture = concatVectorsInPlace(params_with_capture, captures);

  cel0_Value* result = createVectorValue();
  result = appendValueToVectorInPlace(result, params_with_capture);
  return appendValueToVectorInPlace(result, metadata->vector + 1);
}

static cel0_Value* quote(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(stack);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* metadata = lookupVectorMetadata(params->u.vector_id);
  if (metadata->size != 1) return createPanicValue("quote-ill-formed");

  return metadata->vector;
}

static cel0_Value* ifStatement(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);    
  if(params_metadata->size != 3) return createPanicValue("ill-formed");

  cel0_Value* condition_expression = eval(params_metadata->vector, stack);
  if (condition_expression->type == cel0_ValueType_Panic) return condition_expression;
  if (condition_expression->type != cel0_ValueType_Symbol) return createPanicValue("condition-non-symbol");
  
  int true_id = internSymbol("true");
  int false_id = internSymbol("false");  
  int symbol_id = condition_expression->u.symbol_id;
  char condition_true = symbol_id == true_id;
  char condition_false = !condition_true && (symbol_id == false_id);
  assert(condition_true || condition_false);
  assert(condition_true != condition_false);  

  if (condition_true)
    return eval(params_metadata->vector + 1, stack);
  return eval(params_metadata->vector + 2, stack);
}

static cel0_Value* add(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);    
  int result = 0;
  for (int i=0; i<params_metadata->size; i++) {
    if (params_metadata->vector[i].type != cel0_ValueType_Number)
      return createPanicValueWithParam("add-no-number", params_metadata->vector + i);
    result += params_metadata->vector[i].u.number;
  }
  return createNumberValue(result);
}

static cel0_Value* mul(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);    
  int result = 1;
  for (int i=0; i<params_metadata->size; i++) {
    if (params_metadata->vector[i].type != cel0_ValueType_Number)
      return createPanicValue("param-type");
    result *= params_metadata->vector[i].u.number;
  }
  return createNumberValue(result);
}

static cel0_Value* eq(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);      
  if (params_metadata->size != 2) return createPanicValue("eq-ill-formed");
  if (params_metadata->vector[0].type != params_metadata->vector[1].type) return createPanicValue("eq-!=-types");

  char equal = 0;
  int type = params_metadata->vector[0].type;
  if (type == cel0_ValueType_Number) {
    equal = params_metadata->vector[0].u.number == params_metadata->vector[1].u.number;
  } else if (type == cel0_ValueType_Symbol) {
    equal = params_metadata->vector[0].u.symbol_id == params_metadata->vector[1].u.symbol_id;
  } else {
    assert(type == cel0_ValueType_Vector);
    equal = params_metadata->vector[0].u.vector_id == params_metadata->vector[1].u.vector_id;
  }
  return createSymbolValue(equal ? "true" : "false");
}

static cel0_Value* append(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);  
  if (params_metadata->size != 2) return createPanicValue("ill-formed");
  cel0_Value* vec = params_metadata->vector;
  if (vec->type != cel0_ValueType_Vector) return createPanicValue("param-type-1");
  cel0_VectorMetadata* vec_metadata = lookupVectorMetadata(vec->u.vector_id);   
  cel0_Value* result = createVectorValue();
  cel0_VectorMetadata* result_metadata = lookupVectorMetadata(result->u.vector_id);    
  result_metadata->size = vec_metadata->size + 1;
  result_metadata->vector =
    malloc(result_metadata->size * sizeof(cel0_Value));
  memcpy(result_metadata->vector, vec_metadata->vector, vec_metadata->size * sizeof(cel0_Value));
  result_metadata->vector[result_metadata->size - 1] = params_metadata->vector[1];
  return result;
}

static cel0_Value* length(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);
  if (params_metadata->size != 1) return createPanicValue("ill-formed");
  if (params_metadata->vector->type != cel0_ValueType_Vector) return createPanicValue("param-type-1");  
  return createNumberValue(lookupVectorMetadata(params_metadata->vector->u.vector_id)->size);
}

static cel0_Value* nth(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);
  if (params_metadata->size != 2) return createPanicValue("ill-formed");
  cel0_Value* index = params_metadata->vector;
  if (index->type != cel0_ValueType_Number) return createPanicValue("param-type-1");
  cel0_Value* vec = params_metadata->vector + 1;
  if (vec->type != cel0_ValueType_Vector) return createPanicValue("param-type-2");  
  cel0_VectorMetadata* vec_metadata = lookupVectorMetadata(vec->u.vector_id);

  if (index->u.number < 0 || index->u.number >= vec_metadata->size)
    return createPanicValue("out-of-bounds");
  return vec_metadata->vector + index->u.number;  
}

static cel0_Value* open_file(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* params_metadata = lookupVectorMetadata(params->u.vector_id);
  if (params_metadata->size != 1) return createPanicValue("ill-formed");
  cel0_Value* file_name = params_metadata->vector;
  if (file_name->type != cel0_ValueType_Symbol) return createPanicValue("param-type-1");
  FILE* file = fopen(lookupSymbolName(file_name->u.symbol_id), "rb");
  if (!file) return createPanicValue("failed-open");  
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  cel0_Value* buffer = createVectorValue();  
  cel0_VectorMetadata* buffer_metadata = lookupVectorMetadata(buffer->u.vector_id);
  buffer_metadata->size = size;
  buffer_metadata->vector = realloc(buffer_metadata->vector, size * sizeof(cel0_Value));

  for (int i=0; i<size; i++) {
    buffer_metadata->vector[i].type = cel0_ValueType_Number;
    buffer_metadata->vector[i].u.number = 0;
    fread(&buffer_metadata->vector[i].u.number, 1, 1, file);
  }
  fclose(file);
  return buffer;
}

static cel0_Value* vector(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  return params;
}

static cel0_Value* debug_print(cel0_Value* params, cel0_SymbolBindingStack* stack) {
  assert(stack);
  assert(params);
  assert(params->type == cel0_ValueType_Vector);
  cel0_VectorMetadata* metadata = lookupVectorMetadata(params->u.vector_id);
  if (metadata->size != 1) return createPanicValue("ill-formed");
  cel0_printValue(metadata->vector, stdout);
  printf("\n");
  return metadata->vector;
}

#define cel0_SymbolBindingFrameCapacity 1<<20

cel0_Value* cel0_eval(cel0_Value* value) {
  int capacity = cel0_SymbolBindingFrameCapacity;
  cel0_SymbolBinding* frames = malloc(sizeof(cel0_SymbolBinding)*capacity);

  int size = 0;
  int transform = cel0_SymbolBindingType_TransformNative;
  int native = cel0_SymbolBindingType_Native;  

  frames[size++] = (cel0_SymbolBinding)
    { .type = transform, .symbol = createSymbolValue("bind"), .u = {.native  = { bind, bindCaptureLexicalBindings}}};
  frames[size++] = (cel0_SymbolBinding)
    { .type = transform, .symbol = createSymbolValue("lambda"), .u = {.native = { lambda, lambdaCaptureLexicalBindings }}};
  frames[size++] = (cel0_SymbolBinding)
    { .type = transform, .symbol = createSymbolValue("if"), .u = {.native = { ifStatement, ifCaptureLexicalBindings } }}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = transform, .symbol = createSymbolValue("quote"), .u = {.native =  { quote, quoteCaptureLexicalBindings }}};
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("add"), .u = {.native = {add}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("mul"), .u = {.native = {mul}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("eq"), .u = {.native = {eq}}};
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("length"), .u = {.native = {length}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("append"), .u = {.native = {append}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("nth"), .u = {.native = {nth}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("open-file!"), .u = {.native = {open_file}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("dp!"), .u = {.native = {debug_print}}}; 
  frames[size++] = (cel0_SymbolBinding)
    { .type = native, .symbol = createSymbolValue("vec"), .u = {.native = {vector}}}; 
  
  cel0_SymbolBindingStack stack = {.frames = frames, .global_size = size, .begin = size, .size = size, .capacity = capacity };
  
  stack.frames = frames;
  return eval(value, &stack);
}
