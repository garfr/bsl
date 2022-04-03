#include <bsl/parser.h>

const char *builtin_type_to_type_type[] = {
  [TYPE_F32] = "f32",
  [TYPE_F64] = "f64",
  [TYPE_VOID] = "void",
};

TypeType type_start = TYPE_F32;
TypeType type_end = TYPE_VOID;

/* === PROTOTYPES === */

static Expr *parse_expr(Parser *parser);
static Expr *parse_atom_expr(Parser *parser);
static Parameter *parse_parameter(Parser *parser);
static Expr *parse_vector_expr(Parser *parser, Token start_tok, Expr *expr);
static Statement *parse_statement(Parser *parser);
static Expr *parse_record_expr(Parser *parser, Token tok, Expr *expr);
static Toplevel *parse_record_toplevel(Parser *parser, int line, int col);
static Toplevel *parse_procedure(Parser *parser, int line, int col);
static Type *parse_vector_type(Parser *parser, size_t size, Token start);
static Expr *parse_mul_expr(Parser *parser);
static Expr *parse_add_expr(Parser *parser);
static Expr *parse_member_expr(Parser *parser);
static Type *create_type(Parser *parser, TypeType t, int line, int col);
static void parser_error_tok(Parser *parser, Token tok, const char *msg, ...);
static void handle_erratic_tok(Parser *parser, Token tok, 
    const char *expected_item);

static bool expect_with(Parser *parser, TokenType t, const char *expected, Token *tok);
static bool expect(Parser *parser, TokenType t, const char *expected);
static bool token_streq(Token tok, const char *str);

/* === PUBLIC FUNCTIONS === */

bool parser_init(Parser *parser, Lexer *lex, BSLAlloc *alloc, BSLCompileResult *result)
{
  parser->lex = lex;
  parser->result = result;
  parser->alloc = alloc;
  parser->next_entry_point = 0;
  return true;
}

Type *parse_type(Parser *parser)
{
  Token tok = lexer_peek(parser->lex);
  Type *type;
  switch (tok.t)
  {
    case TOKEN_ERR:
      return NULL;
    case TOKEN_EOF:
      parser_error_tok(parser, tok, "unexpected end of file");
      return NULL;
    case TOKEN_SYM: {
      lexer_skip(parser->lex);
      if (token_streq(tok, "vec2"))
      {
        return parse_vector_type(parser, 2, tok);
      } else if (token_streq(tok, "vec3"))
      {
        return parse_vector_type(parser, 3, tok);
      } else if (token_streq(tok, "vec4"))
      {
        return parse_vector_type(parser, 4, tok);
      }
      type = create_type(parser, TYPE_VAR, tok.line, tok.col);
      type->line = tok.line;
      type->col = tok.col;
      type->var.name = tok.sym.data;
      type->var.name_len = tok.sym.size;
      for (TypeType i = type_start; i <= type_end; i++)
      {
        size_t size = strlen(builtin_type_to_type_type[i]);
        if (size == tok.sym.size && strncmp(builtin_type_to_type_type[i], 
              tok.sym.data, size) == 0)
        {
          type->t = i;
        }
      }
      return type;
    }
    default:
      parser_error_tok(parser, tok, "expected type");
      return NULL;
  }
}

bool parse_toplevel_attr(Parser *parser)
{
  Token attr_tok;
  if (!expect_with(parser, TOKEN_SYM, "attribute name", &attr_tok))
  {
    return false;
  }

  if (token_streq(attr_tok, "entry_point"))
  {
    Token entry_tok;
    if (!expect(parser, TOKEN_LPAREN, "entry point name"))
    {
      return false;
    }

    if (!expect_with(parser, TOKEN_SYM, "entry point name", &entry_tok))
    {
      return false;
    }

    if (token_streq(entry_tok, "vertex"))
    {
      parser->next_entry_point |= ENTRY_POINT_VERTEX;
    } else if (token_streq(entry_tok, "fragment"))
    {
      parser->next_entry_point |= ENTRY_POINT_FRAGMENT;
    } else
    {
      parser_error_tok(parser, entry_tok, "unknown entry point '%.*s'", entry_tok.sym.size, entry_tok.sym.data);
      return false;
    }

    if (!expect(parser, TOKEN_RPAREN, "right parenthesis"))
    {
      return false;
    }
  } else
  {
      parser_error_tok(parser, attr_tok, "unknown attribute '%.*s'", attr_tok.sym.size, attr_tok.sym.data);
      return false;
  }

  if (!expect(parser, TOKEN_RBRACK, "right bracket"))
  {
    return false;
  }

  return true;
}

Toplevel *parse_toplevel(Parser *parser)
{
  Token tok;
  while ((tok = lexer_peek(parser->lex)).t == TOKEN_LBRACK)
  {
    lexer_skip(parser->lex);
    if (!parse_toplevel_attr(parser))
    {
      return NULL;
    }
  }

  switch (tok.t)
  {
    case TOKEN_KW_RECORD:
      lexer_skip(parser->lex);
      return parse_record_toplevel(parser, tok.line, tok.col);
    case TOKEN_KW_PROC:
      lexer_skip(parser->lex);
      return parse_procedure(parser, tok.line, tok.col);
    case TOKEN_ERR:
      return NULL;
    default:
      parser_error_tok(parser, tok, "expected toplevel");
      return NULL;
  }
}

bool parse_ast(Parser *parser, AST *ast)
{
  parser->ast = ast;
  ast->alloc = parser->alloc;
  ast->result = parser->result;

  ast->type_scope.entries = NULL;
  ast->type_scope.up = NULL;

  Token tok;
  ast->toplevels = NULL;
  while ((tok = lexer_peek(parser->lex)).t != TOKEN_EOF && tok.t != TOKEN_ERR)
  {
    Toplevel *toplvl = parse_toplevel(parser);
    if (toplvl == NULL)
    {
      return false;
    }
    toplvl->next = ast->toplevels;
    ast->toplevels = toplvl;
  }

  if (tok.t == TOKEN_ERR)
  {
    return false;
  }

  Toplevel *tmp, *follow, *first;
  first = ast->toplevels;
  follow = tmp = NULL;
  while (first != NULL)
  {
    tmp = first->next;
    first->next = follow;
    follow = first;
    first = tmp;
  }
  ast->toplevels = follow;

  return true;
}

/* === PRIVATE FUNCTIONS === */

static Expr *parse_record_expr(Parser *parser, Token start_tok, Expr *expr)
{
  Token tok, name_tok;
  expr->t = EXPR_RECORD;
  expr->line = start_tok.line;
  expr->col = start_tok.col;
  if (!expect_with(parser, TOKEN_SYM, "record name", &name_tok))
  {
    return NULL;
  }

  expr->record.members = NULL;
  expr->record.name = name_tok.sym.data;
  expr->record.name_len = name_tok.sym.size;
  while ((tok = lexer_next(parser->lex)).t == TOKEN_PERIOD)
  {
    RecordExprMember *member = BSL_NEW(parser->alloc, RecordExprMember);
    Token member_name;
    if (!expect_with(parser, TOKEN_SYM, "member name", &member_name))
    {
      return NULL;
    }
    member->name = member_name.sym.data;
    member->name_len = member_name.sym.size;
    member->line = member_name.line;
    member->col = member_name.col;

    if (!expect(parser, TOKEN_EQ, "'='"))
    {
      return NULL;
    }

    member->expr = parse_expr(parser);

    if (member->expr == NULL)
    {
      return NULL;
    }

    if (!expect(parser, TOKEN_COMMA, "','"))
    {
      return NULL;
    }

    member->next = expr->record.members;
    expr->record.members = member;
  }

  if (tok.t != TOKEN_KW_END)
  {
    handle_erratic_tok(parser, tok, "record member");
    return NULL;
  }

  return expr;
}

static Expr *parse_vector_expr(Parser *parser, Token start_tok, Expr *expr)
{
  Token tok;
  expr->t = EXPR_VECTOR;
  expr->line = start_tok.line;
  expr->col = start_tok.col;

  expr->vec.exprs = parse_expr(parser);
  if (expr->vec.exprs == NULL)
  {
    return NULL;
  }

  expr->vec.exprs->next = NULL;

  while ((tok = lexer_next(parser->lex)).t == TOKEN_COMMA)
  {
    Expr *tmp = parse_expr(parser);
    if (tmp == NULL)
    {
      return NULL;
    }

    tmp->next = expr->vec.exprs;
    expr->vec.exprs = tmp;
  }

  if (tok.t != TOKEN_RCURLY)
  {
    handle_erratic_tok(parser, tok, "comma");
    return NULL;
  }

  Expr *first, *follow, *tmp;
  first = expr->vec.exprs;
  follow = tmp = NULL;
  while (first != NULL)
  {
    tmp = first->next;
    first->next = follow;
    follow = first;
    first = tmp;
  }
  expr->vec.exprs = follow;

  return expr;
}

static Expr *parse_expr(Parser *parser)
{
  return parse_add_expr(parser);
}

static Expr *parse_add_expr(Parser *parser)
{
  Expr *lhs = parse_mul_expr(parser);
  if (lhs == NULL)
  {
    return NULL;
  }

  Token tok;
  while ((tok = lexer_peek(parser->lex)).t == TOKEN_ADD || tok.t == TOKEN_SUB)
  {
    lexer_skip(parser->lex);
    Binop op = tok.t == TOKEN_ADD ? BINOP_ADD : TOKEN_SUB;
    Expr *rhs = parse_mul_expr(parser);
    if (rhs == NULL)
    {
      return NULL;
    }

    Expr *new = BSL_NEW(parser->alloc, Expr);
    new->t = EXPR_BINARY;
    new->line = lhs->line;
    new->col = lhs->col;
    new->binary.rhs = rhs;
    new->binary.lhs = lhs;
    new->binary.op = op;

    lhs = new;
  }
  return lhs;
}

static Expr *parse_mul_expr(Parser *parser)
{
  Expr *lhs = parse_member_expr(parser);
  if (lhs == NULL)
  {
    return NULL;
  }

  Token tok;
  while ((tok = lexer_peek(parser->lex)).t == TOKEN_MUL || tok.t == TOKEN_DIV)
  {
    lexer_skip(parser->lex);
    Binop op = tok.t == TOKEN_MUL ? BINOP_MUL : TOKEN_DIV;
    Expr *rhs = parse_member_expr(parser);
    if (rhs == NULL)
    {
      return NULL;
    }

    Expr *new = BSL_NEW(parser->alloc, Expr);
    new->t = EXPR_BINARY;
    new->line = lhs->line;
    new->col = lhs->col;
    new->binary.rhs = rhs;
    new->binary.lhs = lhs;
    new->binary.op = op;

    lhs = new;
  }
  return lhs;
}

static Expr *parse_member_expr(Parser *parser)
{
  Token tok;
  Expr *lhs = parse_atom_expr(parser);
  if (lhs == NULL)
  {
    return NULL;
  }

  while ((tok = lexer_peek(parser->lex)).t == TOKEN_PERIOD)
  {
    lexer_next(parser->lex);
    Token member_tok;
    if (!expect_with(parser, TOKEN_SYM, "member name", &member_tok))
    {
      return NULL;
    }
    Expr *new = BSL_NEW(parser->alloc, Expr);
    new->t = EXPR_MEMBER;
    new->line = lhs->line;
    new->col = lhs->col;
    new->member.lhs = lhs;
    new->member.name = member_tok.sym.data;
    new->member.name_len = member_tok.sym.size;

    lhs = new;
  }

  return lhs;
}

static Expr *parse_atom_expr(Parser *parser)
{
  Expr *expr = BSL_NEW(parser->alloc, Expr);
  Token tok = lexer_peek(parser->lex);
  expr->line = tok.line;
  expr->col = tok.col;
  switch (tok.t)
  {
    case TOKEN_KW_RECORD:
      lexer_skip(parser->lex);
      return parse_record_expr(parser, tok, expr);
    case TOKEN_LCURLY:
      lexer_skip(parser->lex);
      return parse_vector_expr(parser, tok, expr);
    case TOKEN_LPAREN: {
      lexer_skip(parser->lex);
      Expr *expr = parse_expr(parser); 
      if (!expect(parser, TOKEN_RPAREN, "right parenthesis"))
      {
        return false;
      }
      return expr;
    }
    case TOKEN_NUM:
      lexer_skip(parser->lex);
      expr->t = EXPR_NUM;
      expr->num = tok.num;
      return expr;
    case TOKEN_SYM: {
      lexer_skip(parser->lex);
      expr->t = EXPR_VAR;
      expr->var.name = tok.sym.data;
      expr->var.name_len = tok.sym.size;
      return expr;
    }
    default:
      handle_erratic_tok(parser, tok, "expression");
      return NULL;
  }
}

static Statement *parse_statement(Parser *parser)
{
  Statement *stmt = BSL_NEW(parser->alloc, Statement);
  Token tok = lexer_peek(parser->lex);
  stmt->line = tok.line;
  stmt->col = tok.col;
  switch (tok.t)
  {
    case TOKEN_KW_VAR: {
      stmt->t = STATEMENT_VAR;
      lexer_skip(parser->lex);
      Token name_tok;
      if (!expect_with(parser, TOKEN_SYM, "variable name", &name_tok))
      {
        return NULL;
      }

      stmt->var.name = name_tok.sym.data;
      stmt->var.name_len = name_tok.sym.size;

      if (lexer_peek(parser->lex).t == TOKEN_COLON)
      {
        lexer_skip(parser->lex);
        stmt->var.type = parse_type(parser);
        if (stmt->var.type == NULL)
        {
          return false;
        }
      }

      if (!expect(parser, TOKEN_EQ, "'='"))
      {
        return NULL;
      }

      stmt->var.expr = parse_expr(parser);
      if (stmt->var.expr == NULL)
      {
        return NULL;
      }
      return stmt;
    }
    case TOKEN_KW_RETURN: {
      stmt->t = STATEMENT_RETURN;
      lexer_skip(parser->lex);
      stmt->ret.expr = parse_expr(parser);
      if (stmt->ret.expr == NULL)
      {
        return NULL;
      }
      return stmt;
    }
    default:
      handle_erratic_tok(parser, tok, "statement");
      return NULL;
  }

}

static Parameter *parse_parameter(Parser *parser)
{
  Parameter *param = BSL_NEW(parser->alloc, Parameter);
  Token name_tok;
  if (!expect_with(parser, TOKEN_SYM, "parameter name", &name_tok))
  {
    return NULL;
  }

  param->line = name_tok.line;
  param->col = name_tok.col;
  param->name = name_tok.sym.data;
  param->name_len = name_tok.sym.size;

  if (!expect(parser, TOKEN_COLON, "':'"))
  {
    return NULL;
  }
  
  param->type = parse_type(parser);
  if (param->type == NULL)
  {
    return NULL;
  }

  return param;
}

static Toplevel *parse_procedure(Parser *parser, int line, int col)
{
  Toplevel *toplevel = BSL_NEW(parser->alloc, Toplevel);
  toplevel->t = TOPLEVEL_PROC;
  toplevel->line = line;
  toplevel->col = col;
  toplevel->proc.stmts = NULL;

  Token name_tok;
  if (!expect_with(parser, TOKEN_SYM, "procedure name", &name_tok))
  {
    return NULL;
  }
  toplevel->proc.name = name_tok.sym.data;
  toplevel->proc.name_len = name_tok.sym.size;

  if (!expect(parser, TOKEN_LPAREN, "function arguments"))
  {
    return NULL;
  }

  Token tok;
  if (lexer_peek(parser->lex).t == TOKEN_RPAREN)
  {
    toplevel->proc.params = NULL;
    lexer_skip(parser->lex);
  } else 
  {
    toplevel->proc.params = parse_parameter(parser);
    if (toplevel->proc.params == NULL)
    {
      return NULL;
    }

    toplevel->proc.params->next = NULL;
    while ((tok = lexer_next(parser->lex)).t == TOKEN_COMMA)
    {
      Parameter *tmp = parse_parameter(parser);
      if (tmp == NULL)
      {
        return NULL;
      }

      tmp->next = toplevel->proc.params;
      toplevel->proc.params = tmp;
    }

    if (tok.t != TOKEN_RPAREN)
    {
      handle_erratic_tok(parser, tok, "funtion parameter");
      return NULL;
    }
  }

  toplevel->proc.return_type = parse_type(parser);
  if (toplevel->proc.return_type == NULL)
  {
    return NULL;
  }

  while ((tok = lexer_peek(parser->lex)).t != TOKEN_KW_END && 
      tok.t != TOKEN_EOF && tok.t != TOKEN_ERR)
  {
    Statement *stmt = parse_statement(parser);
    if (stmt == NULL)
    {
      return NULL;
    }

    stmt->next = toplevel->proc.stmts;
    toplevel->proc.stmts = stmt;
  }

  Statement *first, *follow, *tmp;
  first = toplevel->proc.stmts;
  follow = tmp = NULL;
  while (first != NULL)
  {
    tmp = first->next;
    first->next = follow;
    follow = first;
    first = tmp;
  }
  toplevel->proc.stmts = follow;

  if (tok.t != TOKEN_KW_END)
  {
    handle_erratic_tok(parser, tok, "statement");
  }
  lexer_skip(parser->lex);

  toplevel->proc.entry_point = parser->next_entry_point;
  parser->next_entry_point = 0;
  return toplevel;
}

static Toplevel *parse_record_toplevel(Parser *parser, int line, int col)
{
  Token name_tok;
  if (!expect_with(parser, TOKEN_SYM, "record name", &name_tok))
  {
    return NULL;
  }

  Toplevel *toplvl = BSL_NEW(parser->alloc, Toplevel);

  toplvl->record.entries = NULL;
  toplvl->record.name = name_tok.sym.data;
  toplvl->record.name_len = name_tok.sym.size;
  toplvl->t = TOPLEVEL_RECORD;
  toplvl->line = line;
  toplvl->col = col;
  Token sym_tok;
  while ((sym_tok = lexer_next(parser->lex)).t == TOKEN_SYM || 
      sym_tok.t == TOKEN_LBRACK)
  {
    RecordEntry *entry = BSL_NEW(parser->alloc, RecordEntry);
    if (sym_tok.t == TOKEN_LBRACK)
    {
      Token attr_tok;
      if (!expect_with(parser, TOKEN_SYM, "attribute name", &attr_tok))
      {
        return NULL;
      }
      
      if (token_streq(attr_tok, "builtin"))
      {
        Token builtin_tok;
        entry->t = RECORD_ENTRY_BUILTIN;
        if (!expect(parser, TOKEN_LPAREN, "left parenthesis"))
        {
          return NULL;
        }

        if (!expect_with(parser, TOKEN_SYM, "name of builtin", &builtin_tok))
        {
          return NULL;
        }

        if (token_streq(builtin_tok, "position"))
        {
          entry->builtin = BUILTIN_CLIP_POSITION;
        } else
        {
          parser_error_tok(parser, builtin_tok, 
              "unknown builtin name: '%.*s'", builtin_tok.sym.size, 
              builtin_tok.sym.data);
          return NULL;
        } 

        if (!expect(parser, TOKEN_RPAREN, "right parenthesis"))
        {
          return NULL;
        }
      } else if (token_streq(attr_tok, "output"))
      {
        Token binding_tok;
        entry->t = RECORD_ENTRY_OUTPUT;

        if (!expect(parser, TOKEN_LPAREN, "left parenthesis"))
        {
          return NULL;
        }

        if (!expect_with(parser, TOKEN_NUM, "input binding", &binding_tok))
        {
          return NULL;
        }

        if (binding_tok.num.t != NUMBER_INT)
        {
          parser_error_tok(parser, binding_tok,
              "binding must be an integer");
          return NULL;
        }

        if (!expect(parser, TOKEN_RPAREN, "right parenthesis"))
        {
          return NULL;
        }
        entry->pos = binding_tok.num.i;
      } else if (token_streq(attr_tok, "input"))
      {
        Token binding_tok;
        entry->t = RECORD_ENTRY_INPUT;

        if (!expect(parser, TOKEN_LPAREN, "left parenthesis"))
        {
          return NULL;
        }

        if (!expect_with(parser, TOKEN_NUM, "input binding", &binding_tok))
        {
          return NULL;
        }

        if (binding_tok.num.t != NUMBER_INT)
        {
          parser_error_tok(parser, binding_tok,
              "binding must be an integer");
          return NULL;
        }

        if (!expect(parser, TOKEN_RPAREN, "right parenthesis"))
        {
          return NULL;
        }
        entry->pos = binding_tok.num.i;
      } else
      {
        parser_error_tok(parser, attr_tok, 
            "unknown attribute name: '%.*s'", attr_tok.sym.size, 
            attr_tok.sym.data);
        return NULL;
      }

      if (!expect(parser, TOKEN_RBRACK, "right bracket"))
      {
        return NULL;
      }

      if (!expect_with(parser, TOKEN_SYM, "member name", &sym_tok))
      {
        return NULL;
      }
    } else
    {
      entry->t = RECORD_ENTRY_NORMAL;
    }

    if (!expect(parser, TOKEN_COLON, "':'"))
    {
      return NULL;
    }

    entry->type = parse_type(parser);
    if (entry->type == NULL)
    {
      return NULL;
    }

    entry->name = sym_tok.sym.data;
    entry->name_len = sym_tok.sym.size;
    entry->next = toplvl->record.entries;
    toplvl->record.entries = entry;
  }   

  if (sym_tok.t != TOKEN_KW_END)
  {
    handle_erratic_tok(parser, sym_tok, "record member");
    return NULL;
  } 
  return toplvl;
}

static Type *parse_vector_type(Parser *parser, size_t size, Token start)
{
  if (!expect(parser, TOKEN_LT, "vector parameter"))
  {
    return NULL;
  }

  Type *subtype = parse_type(parser);
  if (subtype == NULL)
  {
    return NULL;
  }

  if (!expect(parser, TOKEN_GT, "closing angled bracket"))
  {
    return NULL;
  }

  Type *type = create_type(parser, TYPE_VECTOR, start.line, start.col);
  type->vec.size = size;
  type->vec.type = subtype;
  return type;
}

static Type *create_type(Parser *parser, TypeType t, int line, int col)
{
  Type *type = BSL_NEW(parser->alloc, Type);
  type->t = t;
  type->line = line;
  type->col = col;
  return type;
}

static void parser_error_tok(Parser *parser, Token tok, const char *msg, ...)
{
  va_list args;

  va_start(args, msg);
  vresult_error(parser->result, tok.line, tok.col, msg, args);
}

static bool expect_with(Parser *parser, TokenType t, const char *expected, Token *tok_out)
{
  Token tok = lexer_next(parser->lex);
  *tok_out = tok;
  if (tok.t == t)
  {
    return true;
  } else
  {
    handle_erratic_tok(parser, tok, expected);
    return false;
  }
}

static bool expect(Parser *parser, TokenType t, const char *expected)
{
  Token tok = lexer_next(parser->lex);
  if (tok.t == t)
  {
    return true;
  } else
  {
    handle_erratic_tok(parser, tok, expected);
    return false;
  }
}

static void handle_erratic_tok(Parser *parser, Token tok, const char *expected_item)
{
  if (tok.t == TOKEN_EOF)
  {
    parser_error_tok(parser, tok, "unexpected end of file");
  } else if (tok.t != TOKEN_ERR)
  {
    parser_error_tok(parser, tok, "expected %s", expected_item);
  }
}

static bool token_streq(Token tok, const char *str)
{
  size_t size = strlen(str);
  return size == tok.sym.size && strncmp(str, tok.sym.data, size) == 0;
}
