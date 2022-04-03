#include <bsl.h>

#include <bsl/lexer.h>
#include <bsl/parser.h>
#include <bsl/resolve.h>

bool bsl_compile(BSLCompileInfo *compile_info, BSLCompileResult *result)
{
  Lexer lexer; 
  Parser parser;
  AST ast;
  BSLAlloc alloc = {
    .ud = compile_info->internal_ud,
    .fn = compile_info->internal_fn,
  };

  if (!lexer_init(&lexer, compile_info->src, compile_info->src_len, result))
  {
    return false;
  }

  if (!parser_init(&parser, &lexer, &alloc, result))
  {
    return false;
  }

  if (!parse_ast(&parser, &ast))
  {
    return false;
  }

  if (!resolve_names(&ast))
  {
    return false;
  }

  return true;
}
