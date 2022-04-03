#ifndef BSL_H
#define BSL_H

#define BSL_RESULT_MAX_MESSAGE_LEN 512

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef void*(*BSLAllocFn)(void *ptr, size_t osz, size_t nsz, void *ud);

typedef struct
{
  int line, col;
  char msg[BSL_RESULT_MAX_MESSAGE_LEN];
} BSLCompileResult;

typedef struct
{
  void *internal_ud;
  BSLAllocFn internal_fn;
  const uint8_t *src;
  size_t src_len;
} BSLCompileInfo;

bool bsl_compile(BSLCompileInfo *compile_info, BSLCompileResult *result);

#endif
