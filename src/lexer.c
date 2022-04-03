#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

#include <bsl/lexer.h>

#define PEEK_C(_lexer) ((_lexer)->src[(_lexer)->cur])
#define NEXT_C(_lexer) ((_lexer)->col++, (_lexer)->src[(_lexer)->cur++])
#define SKIP_C(_lexer) ((_lexer)->col++, (_lexer)->cur++)
#define IS_EOF(_lexer) ((_lexer)->cur >= (_lexer)->src_len)
#define RESET(_lexer) ((_lexer)->start = (_lexer)->cur)

const char *keyword_to_token_type[] = {
  [TOKEN_KW_PROC] = "proc",
  [TOKEN_KW_RECORD] = "record",
  [TOKEN_KW_VAR] = "var",
  [TOKEN_KW_RETURN] = "return",
  [TOKEN_KW_END] = "end",
};

TokenType keyword_start = TOKEN_KW_PROC;
TokenType keyword_end = TOKEN_KW_END;

/* === PROTOTYPES === */

static Token next_token(Lexer *lexer);
static void fill_token(Token *tok, Lexer *lexer, TokenType t);
static void lexer_error(Lexer *lexer, const char *msg, ...);
static bool skip_whitespace(Lexer *lexer);
static Token lex_sym(Lexer *lexer);
static Token lex_num(Lexer *lexer);

/* === PUBLIC FUNCTIONS === */

bool lexer_init(Lexer *lexer,
    const uint8_t *src, size_t src_len, BSLCompileResult *result)
{
  lexer->src = src;
  lexer->src_len = src_len;
  lexer->result = result;
  lexer->has_peek = false;
  lexer->line = lexer->col = 1;
  lexer->start = lexer->cur = 0;
  return true;
}

Token lexer_next(Lexer *lexer)
{
  if (lexer->has_peek)
  {
    lexer->has_peek = false;
    return lexer->peek;
  }
  return next_token(lexer);
}

Token lexer_peek(Lexer *lexer)
{
  if (lexer->has_peek)
  {
    return lexer->peek;
  }
  lexer->has_peek = true;
  return lexer->peek = next_token(lexer);
}

void lexer_skip(Lexer *lexer)
{
  if (lexer->has_peek)
  {
    lexer->has_peek = false;
  } else
  {
    next_token(lexer);
  }
}

void lexer_print(Token tok)
{
  switch (tok.t)
  {
    case TOKEN_SYM:
      printf("Sym: '%.*s'\n", (int) tok.sym.size, (const char *) tok.sym.data);
      break;
    case TOKEN_NUM:
      if (tok.num.t == NUMBER_INT)
      {
        printf("Num: %" PRId64 "\n", tok.num.i);
      } else
      {
        printf("Num: %f\n", tok.num.f);
      }
      break;
    case TOKEN_KW_PROC:
      printf("Proc\n");
      break;
    case TOKEN_KW_RECORD:
      printf("Record\n");
      break;
    case TOKEN_KW_VAR:
      printf("Var\n");
      break;
    case TOKEN_KW_RETURN:
      printf("Return\n");
      break;
    case TOKEN_KW_END:
      printf("End\n");
      break;
    case TOKEN_COMMA:
      printf("Comma\n");
      break;
    case TOKEN_ADD:
      printf("Add\n");
      break;
    case TOKEN_SUB:
      printf("Sub\n");
      break;
    case TOKEN_MUL:
      printf("Mul\n");
      break;
    case TOKEN_DIV:
      printf("Div\n");
      break;
    case TOKEN_EQ:
      printf("Eq\n");
      break;
    case TOKEN_PERIOD:
      printf("Period\n");
      break;
    case TOKEN_ARROW:
      printf("Arrow\n");
      break;
    case TOKEN_LT:
      printf("LessThan\n");
      break;
    case TOKEN_GT:
      printf("GreaterThan\n");
      break;
    case TOKEN_COLON:
      printf("Colon\n");
      break;
    case TOKEN_NEWLINE:
      printf("Newline\n");
      break;
    case TOKEN_SEMICOLON:
      printf("Semicolon\n");
      break;
    case TOKEN_LCURLY:
      printf("Left Curly\n");
      break;
    case TOKEN_RCURLY:
      printf("Right Curly\n");
      break;
    case TOKEN_LBRACK:
      printf("Left Bracket\n");
      break;
    case TOKEN_RBRACK:
      printf("Right Bracket\n");
      break;
    case TOKEN_LPAREN:
      printf("Left Paren\n");
      break;
    case TOKEN_RPAREN:
      printf("Right Paren\n");
      break;
    case TOKEN_EOF:
      printf("EOF\n");
      break;
    case TOKEN_ERR:
      printf("ERR\n");
      break;
    default:
      printf("Could not print token type: %d\n", tok.t);
      break;
  }
}

/* === PRIVATE FUNCTIONS === */

static Token next_token(Lexer *lexer)
{
  Token tok;

  if (!skip_whitespace(lexer))
  {
    fill_token(&tok, lexer, TOKEN_EOF);
    return tok;
  }

  int c = PEEK_C(lexer);
  while (c == '#')
  {
    while (!IS_EOF(lexer) && NEXT_C(lexer) != '\n')
      ;
    if (IS_EOF(lexer))
    {
      fill_token(&tok, lexer, TOKEN_EOF);
      return tok;
    }

    lexer->col = 1;
    lexer->line++;

    if (!skip_whitespace(lexer))
    {
      fill_token(&tok, lexer, TOKEN_EOF);
      return tok;
    }
    c = PEEK_C(lexer);
  }

  if (isalpha(c) || c == '_')
  {
    return lex_sym(lexer);
  }

  if (isdigit(c))
  {
    return lex_num(lexer);
  }

  switch (c)
  {
    case ':':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_COLON);
      return tok;
    case '.':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_PERIOD);
      return tok;
    case ',':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_COMMA);
      return tok;
    case '=':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_EQ);
      return tok;
    case '+':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_ADD);
      return tok;
    case '-':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_SUB);
      return tok;
    case '*':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_MUL);
      return tok;
    case '/':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_DIV);
      return tok;
    case '>':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_GT);
      return tok;
    case '<':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_LT);
      return tok;
    case '{':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_LCURLY);
      return tok;
    case '}':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_RCURLY);
      return tok;
    case '[':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_LBRACK);
      return tok;
    case ']':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_RBRACK);
      return tok;
    case '(':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_LPAREN);
      return tok;
    case ')':
      NEXT_C(lexer);
      fill_token(&tok, lexer, TOKEN_RPAREN);
      return tok;
  }

  lexer_error(lexer, "unknown char '%c'", lexer->src[lexer->cur]);
  fill_token(&tok, lexer, TOKEN_ERR);
  return tok;
}

static void fill_token(Token *tok, Lexer *lexer, TokenType t)
{
  tok->t = t;
  tok->line = lexer->line;
  tok->col = lexer->col;
}


static void lexer_error(Lexer *lexer, const char *msg, ...)
{
  va_list args;
  va_start(args, msg);

  vresult_error(lexer->result, lexer->line, lexer->col, msg, args);
}

static bool skip_whitespace(Lexer *lexer)
{
  int c;
  while (!IS_EOF(lexer) && isspace(c = PEEK_C(lexer)))
  {
    SKIP_C(lexer);
    if (c == '\n')
    {
      lexer->col = 1;
      lexer->line++;
    }
  }
  RESET(lexer);
  return !IS_EOF(lexer);
}

static Token lex_sym(Lexer *lexer)
{
  Token tok;
  int c;

  while (!IS_EOF(lexer) && (isalpha(c = PEEK_C(lexer)) || isdigit(c) || 
        c == '_'))
  {
    NEXT_C(lexer);
  }

  fill_token(&tok, lexer, TOKEN_SYM);
  tok.sym.data = lexer->src + lexer->start;
  tok.sym.size = lexer->cur - lexer->start;
  for (TokenType i = keyword_start; i <= keyword_end; i++)
  {
    size_t size = strlen(keyword_to_token_type[i]);
    if (size == tok.sym.size && strncmp(tok.sym.data, 
          keyword_to_token_type[i], size) == 0)
    {
      tok.t = i;
    }
  }

  RESET(lexer);
  return tok;
}

static Token lex_num(Lexer *lexer)
{
  Token tok;
  int c;
  int64_t i = 0;

  while (!IS_EOF(lexer) && isdigit(c = PEEK_C(lexer)))
  {
    i *= 10;
   i += (c - '0');
    NEXT_C(lexer);
  }
  if (!IS_EOF(lexer) && c == '.')
  {
    NEXT_C(lexer);
    double frac = 0.0;
    double place = 10.0;
    while (!IS_EOF(lexer) && isdigit(c = PEEK_C(lexer)))
    {
      frac += (c - '0') / place;
      place *= 10.0;
      NEXT_C(lexer);
    }

    fill_token(&tok, lexer, TOKEN_NUM);
    tok.num.t = NUMBER_REAL;
    tok.num.f = (double) i + frac;
    RESET(lexer);
    return tok;
  }

  fill_token(&tok, lexer, TOKEN_NUM);
  tok.num.t = NUMBER_INT;
  tok.num.i = i;
  RESET(lexer);
  return tok;
}
