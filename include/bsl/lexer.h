#ifndef BSL_LEXER_H
#define BSL_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <bsl.h>
#include <bsl/util.h>

typedef enum 
{
  TOKEN_SYM,
  TOKEN_NUM,

  TOKEN_KW_PROC,
  TOKEN_KW_RECORD,
  TOKEN_KW_VAR,
  TOKEN_KW_RETURN,
  TOKEN_KW_END,

  TOKEN_COMMA,
  TOKEN_PERIOD,
  TOKEN_ARROW,

  TOKEN_EQ,
  TOKEN_LT,
  TOKEN_GT,

  TOKEN_ADD,
  TOKEN_SUB,
  TOKEN_MUL,
  TOKEN_DIV,

  TOKEN_COLON,

  TOKEN_NEWLINE,
  TOKEN_SEMICOLON,

  TOKEN_LBRACK,
  TOKEN_RBRACK,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LCURLY,
  TOKEN_RCURLY,

  TOKEN_EOF,
  TOKEN_ERR,
} TokenType;


typedef struct
{
  TokenType t;
  int line, col;

  union {
    struct {
      const char *data;
      size_t size;
    } sym;
    Number num;
  };
} Token;

typedef struct
{
  const uint8_t *src;
  size_t src_len;
  BSLCompileResult *result;
  bool has_peek;
  Token peek;
  int line, col;
  size_t cur, start;
} Lexer;

bool lexer_init(Lexer *lexer, const uint8_t *src, size_t src_len, 
    BSLCompileResult *result);

Token lexer_next(Lexer *lexer);
Token lexer_peek(Lexer *lexer);
void lexer_skip(Lexer *lexer);

void lexer_print(Token tok);

#endif
