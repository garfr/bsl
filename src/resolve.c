#include <bsl/resolve.h>
#include <bsl/util.h>

/* === PROTOTYPES === */

static VarEntry *add_to_scope(AST *ast, Scope *scope, const uint8_t *name, size_t name_len);
static VarEntry *lookup_scope(Scope *scope, const uint8_t *name, size_t name_len);
static bool resolve_proc(AST *ast, Toplevel *proc);
static bool resolve_statement(AST *ast, Scope *scope, Statement *stmt, Type **type);
static bool resolve_expr(AST *ast, Scope *scope, Expr *expr);
static bool resolve_record_expr(AST *ast, Scope *scope, Expr *expr);
static bool compare_types(AST *ast, int line, int col, Type *type1, Type *type2);
static bool resolve_type(AST *ast, int line, int col, Type **_type);

/* === PUBLIC FUNCTIONS === */

bool resolve_names(AST *ast)
{
  ast->scope.up = NULL;
  ast->scope.entries = NULL;

  Toplevel *iter = ast->toplevels;

  while (iter != NULL)
  {
    switch (iter->t)
    {
      case TOPLEVEL_PROC:
        iter->proc.entry = add_to_scope(ast, &ast->scope, iter->proc.name, 
              iter->proc.name_len);
        if (iter->proc.entry == NULL)
        {
          result_error(ast->result, iter->line, iter->col, 
              "redeclaration of toplevel '%.*s'", iter->proc.name_len, 
              iter->proc.name);
          return false;
        } 
        break;
      case TOPLEVEL_RECORD:
        iter->record.entry = add_to_scope(ast, &ast->type_scope, iter->record.name,
            iter->record.name_len);
        if (iter->record.entry == NULL)
        {
          result_error(ast->result, iter->line, iter->col,
              "redeclaration of record type '%.*s'", iter->record.name_len,
              iter->record.name);
          return false;
        }
        Type *new_type = BSL_NEW(ast->alloc, Type);
        new_type->t = TYPE_RECORD;
        new_type->record.entries = iter->record.entries;
        new_type->record.name = iter->record.name;
        new_type->record.name_len = iter->record.name_len;
        iter->record.entry->type = new_type;
        iter->record.entry->record = iter;
        break;
    }
    iter = iter->next;
  }

  iter = ast->toplevels;
  while (iter != NULL)
  {
    if (iter->t == TOPLEVEL_PROC)
    {
      if (!resolve_proc(ast, iter))
      {
        return false;
      }
    }

    iter = iter->next;
  }

  return true;
}

/* === PRIVATE FUNCTIONS === */

static VarEntry *add_to_scope(AST *ast, Scope *scope, const uint8_t *name, 
    size_t name_len)
{
  Scope *scope_iter = scope;
  while (scope_iter != NULL)
  {
    VarEntry *iter = scope_iter->entries;
    while (iter != NULL)
    {
      if (name_len == iter->name_len && strncmp(name, iter->name, name_len) == 0)
      {
        return false;
      }
      iter = iter->next;
    }

    scope_iter = scope_iter->up;
  }

  VarEntry *entry = BSL_NEW(ast->alloc, VarEntry);
  entry->name = name;
  entry->name_len = name_len;
  entry->type = NULL;

  entry->next = scope->entries;
  scope->entries = entry;

  return entry;
}

static VarEntry *lookup_scope(Scope *scope, const uint8_t *name, size_t name_len)
{
  Scope *scope_iter = scope;
  while (scope_iter != NULL)
  {
    VarEntry *iter = scope_iter->entries;
    while (iter != NULL)
    {
      if (name_len == iter->name_len && strncmp(name, iter->name, name_len) == 0)
      {
        return iter;
      }
      iter = iter->next;
    }

    scope_iter = scope_iter->up;
  }

  return NULL;
}

static bool resolve_record_expr(AST *ast, Scope *scope, Expr *expr)
{
  expr->record.entry = lookup_scope(&ast->type_scope, 
      expr->record.name, expr->record.name_len);
  if (expr->record.entry == NULL)
  {
    result_error(ast->result, expr->line, expr->col,
        "unknown record type '%.*s'", expr->record.name_len, expr->record.name);
    return false;
  }

  RecordExprMember *iter = expr->record.members;
  while (iter != NULL)
  {
    RecordEntry *iter2 = expr->record.entry->record->record.entries;
    while (iter2 != NULL)
    {
      if (iter2->name_len == iter->name_len && 
          strncmp(iter2->name, iter->name, iter->name_len) == 0)
      {
        iter->entry = iter2;
        break;
      }

      iter2 = iter2->next;
    }
    if (iter->entry == NULL)
    {
      result_error(ast->result, iter->line, iter->col,
          "record type '%.*s' does not have a member '%.*s'",
          expr->record.name_len, expr->record.name,
          iter->name_len, iter->name);
      return false;
    }

    if (!resolve_expr(ast, scope, iter->expr))
    {
      return false;
    }

    if (!compare_types(ast, iter->line, iter->col, iter->expr->type, iter2->type))
    {
      return false;
    }
    iter = iter->next;
  }

  expr->type = expr->record.entry->type;
  return true;
}

static bool resolve_expr(AST *ast, Scope *scope, Expr *expr)
{
  switch (expr->t)
  {
    case EXPR_BINARY: {
      Expr *lhs = expr->binary.lhs;
      Expr *rhs = expr->binary.rhs;

      if (!resolve_expr(ast, scope, lhs))
      {
        return false;
      }
      if (!resolve_expr(ast, scope, rhs))
      {
        return false;
      }

      if (lhs->type->t == TYPE_F32 && rhs->type->t == TYPE_F32 ||
          lhs->type->t == TYPE_F64 && rhs->type->t == TYPE_F64)
      {
        expr->type = lhs->type;
      } else if (lhs->type->t == TYPE_VECTOR && rhs->type->t == TYPE_VECTOR)
      {
        if (!compare_types(ast, expr->line, expr->col, lhs->type->vec.type, rhs->type->vec.type) || 
            lhs->type->vec.size != rhs->type->vec.size)
        {
          result_error(ast->result, expr->line, expr->col,
              "cannot perform arithmetic on vectors of different types or sizes");
          return false;
        }
        expr->type = lhs->type;
      } else if (lhs->type->t == TYPE_VECTOR) {
        if (expr->binary.op == BINOP_ADD || expr->binary.op == BINOP_SUB)
        {
          result_error(ast->result, expr->line, expr->col,
              "cannot perform addition or subtraction on mixed scalar and vector operands");
          return false;
        }
        if (!compare_types(ast, expr->line, expr->col, lhs->type->vec.type, rhs->type))
        {
          result_error(ast->result, expr->line, expr->col,
              "cannot perform vector/scalar multiplication on mixed type operands");
          return false;
        }
        expr->type = lhs->type;
      } else if (rhs->type->t == TYPE_VECTOR) {
        if (expr->binary.op == BINOP_ADD || expr->binary.op == BINOP_SUB)
        {
          result_error(ast->result, expr->line, expr->col,
              "cannot perform addition or subtraction on mixed scalar and vector operands");
          return false;
        }
        if (!compare_types(ast, expr->line, expr->col, rhs->type->vec.type, lhs->type))
        {
          result_error(ast->result, expr->line, expr->col,
              "cannot perform vector/scalar multiplication on mixed type operands");
          return false;
        }
        expr->type = rhs->type;
      } else
      {
        result_error(ast->result, expr->line, expr->col,
            "invalid argument to arithmetic operation");
        return false;
      }
      return true;
    }
    case EXPR_MEMBER: {
      if (!resolve_expr(ast, scope, expr->member.lhs))
      {
        return false;
      }

      if (expr->member.lhs->type->t != TYPE_RECORD)
      {
        result_error(ast->result, expr->line, expr->col,
            "left hand side must be a record type");
        return false;
      }

      Type *rec = expr->member.lhs->type;
      RecordEntry *iter = rec->record.entries;
      while (iter != NULL)
      {
        if (iter->name_len == expr->member.name_len &&
            strncmp(iter->name, expr->member.name, iter->name_len) == 0)
        {
          expr->member.entry = iter;
          break;
        }

        iter = iter->next;
      }
      
      if (expr->member.entry == NULL)
      {
        result_error(ast->result, expr->line, expr->col,
            "record type '%.*s' does not have a member '%.*s'",
            rec->record.name_len, rec->record.name, 
            expr->member.name_len, expr->member.name);
        return false;
      }

      expr->type = expr->member.entry->type;
      return true;
    }

    case EXPR_NUM:
      expr->type = BSL_NEW(ast->alloc, Type);
      expr->type->t = TYPE_F32;
      return true;
    case EXPR_VAR:
      expr->var.entry = lookup_scope(scope, expr->var.name, expr->var.name_len);
      if (expr->var.entry == NULL)
      {
        result_error(ast->result, expr->line, expr->col,
            "variable '%.*s' not in scope", expr->var.name_len, expr->var.name);
        return false;
      }
      expr->type = expr->var.entry->type;
      return true;
    case EXPR_VECTOR: {
      Expr *iter = expr->vec.exprs;
      size_t size = 0;
      if (!resolve_expr(ast, scope, iter))
      {
        return false;
      }
      Type *first_type = iter->type;
      if (first_type->t == TYPE_VECTOR)
      {
        size += first_type->vec.size;
        first_type = first_type->vec.type;
      } else
      {
        size++;
      }

      iter = iter->next;

      while (iter != NULL)
      {
        if (!resolve_expr(ast, scope, iter))
        {
          return false;
        }
        if (iter->type->t == TYPE_VECTOR)
        {
          size += iter->type->vec.size;
        } else
        {
          size++;
        }

        if (!compare_types(ast, expr->line, expr->col, first_type, iter->type))
        {
          return false;
        }

        iter = iter->next;
      }

      if (size > 4)
      {
        result_error(ast->result, expr->line, expr->col,
            "maximum vector size is 4");
        return false;
      }

      Type *new_type = BSL_NEW(ast->alloc, Type);
      new_type->t = TYPE_VECTOR;
      new_type->vec.size = size;
      new_type->vec.type = first_type;
      expr->type = new_type;
      return true;
    }
    case EXPR_RECORD: {
      if (!resolve_record_expr(ast, scope, expr))
      {
        return false;
      }
      return true;
    }
    default:
      return true;
  }
}

static bool resolve_statement(AST *ast, Scope *scope, Statement *stmt, Type **type_out)
{
  *type_out = NULL;

  switch (stmt->t)
  {
    case STATEMENT_VAR:
      {
        stmt->var.entry = add_to_scope(ast, scope, stmt->var.name, stmt->var.name_len);
        if (stmt->var.entry == NULL)
        {
          result_error(ast->result, stmt->line, stmt->col, 
              "redeclaration of variable '%.*s'", stmt->var.name_len, 
              stmt->var.name);
          return false;
        }
        if (stmt->var.expr)
        {
          if (!resolve_expr(ast, scope, stmt->var.expr))
          {
            return false;
          }
        }
        if (stmt->var.type)
        {
          if (!resolve_type(ast, stmt->line, stmt->col, &stmt->var.type))
          {
            return false;
          }
        }
        if (stmt->var.type && stmt->var.expr)
        {
          if (!compare_types(ast, stmt->var.expr->line, stmt->var.expr->col, 
                stmt->var.type, stmt->var.expr->type))
          {
            return false;
          }
        } else if (!stmt->var.type)
        {
          stmt->var.type = stmt->var.expr->type;
        }

        stmt->var.entry->type = stmt->var.type;

        return true;
      }
    case STATEMENT_RETURN:
      if (stmt->ret.expr != NULL)
      {
        if (!resolve_expr(ast, scope, stmt->ret.expr))
        {
          return false;
        }
        *type_out = stmt->ret.expr->type;
      }
      return true;
    default:
      return true;
  }
}

static bool resolve_proc(AST *ast, Toplevel *proc)
{
  proc->proc.scope.up = &ast->scope;
  proc->proc.scope.entries = NULL;

  if (!resolve_type(ast, proc->line, proc->col, &proc->proc.return_type))
  {
    return false;
  }

  proc->proc.entry->type = BSL_NEW(ast->alloc, Type);
  proc->proc.entry->type->t = TYPE_PROC;
  proc->proc.entry->type->proc.return_type = proc->proc.return_type;
  proc->proc.entry->type->proc.params = proc->proc.params;

  Parameter *param = proc->proc.params;
  while (param != NULL)
  {
    VarEntry *entry = add_to_scope(ast, &proc->proc.scope, param->name, 
        param->name_len);
    if (entry == NULL)
    {
      result_error(ast->result, param->line, param->col, 
          "function parameter '%.*s' shadows variable", 
          param->name_len, param->name);
      return false;
    }

    if (!resolve_type(ast, param->line, param->col, &param->type))
    {
      return false;
    }
    entry->type = param->type;
    param = param->next;
  }

  int did_return = false;
  Statement *iter = proc->proc.stmts;
  Type *ret;
  while (iter != NULL)
  {
    if (!resolve_statement(ast, &proc->proc.scope, iter, &ret))
    {
      return false;
    }

    if (ret != NULL)
    {
      if (!compare_types(ast, iter->line, iter->col, ret, proc->proc.return_type))
      {
        result_error(ast->result, iter->line, iter->col,
            "incompatible return type");
        return false;
      } else
      {
        did_return = true;
      }
    }

    iter = iter->next; 
  }

  if (proc->proc.return_type->t != TYPE_VOID && !did_return)
  {
    result_error(ast->result, proc->line, proc->col, "non-void function must return");
    return false;
  }

  return true;
}

static bool compare_types(AST *ast, int line, int col, Type *type1, Type *type2)
{
  if (type1->t != type2->t)
  {
    result_error(ast->result, line, col,
        "incompatible types");
    return false;
  }

  switch (type1->t)
  {
    case TYPE_F32:
    case TYPE_F64:
      return true;
    case TYPE_VECTOR:
      if (!compare_types(ast, line, col, type1->vec.type, type2->vec.type))
      {
        return false;
      }

      if (type1->vec.size != type2->vec.size)
      {
        result_error(ast->result, line, col,
            "different sized vectors");
        return false;
      }
      return true;
    case TYPE_RECORD:
      if (type1 != type2)
      {
        result_error(ast->result, line, col,
            "incompatible record types '%.*s' and '%.*s'",
            type1->record.name_len, type1->record.name,
            type2->record.name_len, type2->record.name);
        return false;
      }
      return true;
    default:
      printf("compare_types: VERY BAD SHOULD NOT HAPPEN");
      return false;
  }
}

static bool resolve_type(AST *ast, int line, int col, Type **_type)
{
  Type *type = *_type;
  switch (type->t)
  {
    case TYPE_VAR:
    {
      VarEntry *entry = lookup_scope(&ast->type_scope, type->var.name, type->var.name_len);
      if (entry == NULL)
      {
        result_error(ast->result, line, col,
            "no type '%.*s' in scope", type->var.name_len, type->var.name);
        return false;
      }
      *_type = entry->type;
      break;
    }
  }
  return true;
}
