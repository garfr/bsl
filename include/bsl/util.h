#ifndef BSL_UTILS_H
#define BSL_UTILS_H

#include <stdarg.h>

#include <bsl.h>

typedef struct
{
  void *ud;
  BSLAllocFn fn;
} BSLAlloc;

typedef enum {
  NUMBER_INT,
  NUMBER_REAL,
} NumberType;

typedef struct 
{
  NumberType t;
  union
  {
    double f;
    int64_t i;
  };
} Number;

#define BSL_NEW(alloc, type) ((alloc)->fn(NULL, 0, sizeof(type), (alloc)->ud))

void result_error(BSLCompileResult *result, int line, int col,
    const char *msg, ...);
void vresult_error(BSLCompileResult *result, int line, int col, 
    const char *msg, va_list args);

#endif
