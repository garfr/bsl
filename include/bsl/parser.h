#ifndef BSL_PARSER_H
#define BSL_PARSER_H

#include <bsl/ast.h>
#include <bsl/lexer.h>
#include <bsl/util.h>

typedef struct
{
  Lexer *lex;
  BSLCompileResult *result;
  BSLAlloc *alloc;
  ProcedureEntryPoint next_entry_point;
  AST *ast;
} Parser;

bool parser_init(Parser *parser, Lexer *lex, BSLAlloc *alloc, BSLCompileResult *result);

Toplevel *parse_toplevel(Parser *parser);
Type *parse_type(Parser *parser);
bool parse_ast(Parser *parser, AST *ast);

#endif
