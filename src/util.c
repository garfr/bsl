#include <string.h>
#include <stdio.h>

#include <bsl/util.h>

void vresult_error(BSLCompileResult *result, int line, int col, 
    const char *msg, va_list args) 
{
  result->line = line;
  result->col = col;
  vsnprintf(result->msg, BSL_RESULT_MAX_MESSAGE_LEN, msg, args); 
}

void result_error(BSLCompileResult *result, int line, int col, 
    const char *msg, ...)
{
  va_list args;
  va_start(args, msg);

  vresult_error(result, line, col, msg, args);
}
