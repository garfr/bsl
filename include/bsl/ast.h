#ifndef BSL_AST_H
#define BSL_AST_H

#include <stdint.h>
#include <stddef.h>

#include <bsl/util.h>

struct Type;
struct Toplevel;

typedef struct Parameter
{
  int line, col;
  const uint8_t *name;
  size_t name_len;
  struct Type *type;
  struct Parameter *next;
} Parameter;

typedef enum
{
  TYPE_F32,
  TYPE_F64,
  TYPE_VOID,

  TYPE_VECTOR,
  TYPE_RECORD,
  TYPE_VAR,
  TYPE_PROC,
} TypeType; 

typedef enum
{
  RECORD_ENTRY_INPUT,
  RECORD_ENTRY_OUTPUT,
  RECORD_ENTRY_BUILTIN,
  RECORD_ENTRY_NORMAL,
} RecordEntryType;

typedef enum
{
  BUILTIN_CLIP_POSITION,
} BuiltinType;

typedef struct RecordEntry
{
  RecordEntryType t;
  union {
    int pos;
    BuiltinType builtin;
  };
  const uint8_t *name;
  size_t name_len;
  struct Type *type;
  struct RecordEntry *next;
} RecordEntry;

typedef struct Type
{
  TypeType t;
  int line, col;
  union {
    struct
    {
      char size;
      struct Type *type;
    } vec;
    struct
    {
      RecordEntry *entries;
      const uint8_t *name;
      size_t name_len;
    } record; 
    struct
    {
      const char *name;
      size_t name_len;
    } var;
    struct
    {
      struct Type *return_type;
      Parameter *params;
    } proc;
  };
} Type;

struct Expr;

typedef struct RecordExprMember
{
  int line, col;
  const char *name;
  size_t name_len;
  struct Expr *expr;
  struct RecordExprMember *next;
  RecordEntry *entry;
} RecordExprMember;

typedef struct VarEntry
{
  const uint8_t *name;
  size_t name_len;
  Type *type;
  struct Toplevel *record;
  struct VarEntry *next;
} VarEntry;

typedef struct Scope
{
  struct VarEntry *entries;
  struct Scope *up;
} Scope;

typedef enum
{
  BINOP_ADD,
  BINOP_MUL,
  BINOP_SUB,
  BINOP_DIV,
} Binop;

typedef enum
{
  EXPR_VAR,
  EXPR_NUM,
  EXPR_RECORD,
  EXPR_MEMBER,
  EXPR_VECTOR,
  EXPR_BINARY,
} ExprType;

typedef struct Expr
{
  ExprType t;
  int line, col;
  union 
  {
    struct
    {
      const uint8_t *name;
      size_t name_len;
      VarEntry *entry;
    } var;
    Number num;
    struct
    {
      const uint8_t *name;
      size_t name_len;
      RecordExprMember *members;
      VarEntry *entry;
    } record;
    struct {
      struct Expr *lhs, *rhs;
      Binop op;
    } binary;
    struct
    {
      struct Expr *lhs;
      const uint8_t *name;
      size_t name_len;
      RecordEntry *entry;
    } member;
    struct
    {
      struct Expr *exprs;
    } vec;
  };
  Type *type;
  struct Expr *next;
} Expr;

typedef enum
{
  STATEMENT_VAR,
  STATEMENT_RETURN,
} StatementType;

typedef struct Statement
{
  StatementType t;
  int line, col;
  union {
    struct
    {
      Expr *expr;
    } ret;
    struct
    {
      VarEntry *entry;
      const uint8_t *name;
      size_t name_len;
      Expr *expr;
      Type *type;
    } var;
  };
  struct Statement *next;
} Statement;

typedef enum
{
  TOPLEVEL_RECORD,
  TOPLEVEL_PROC,
} ToplevelType; 

typedef enum
{
  ENTRY_POINT_VERTEX = 1 << 0,
  ENTRY_POINT_FRAGMENT = 1 << 1,
} ProcedureEntryPoint;

typedef struct Toplevel
{
  ToplevelType t;
  int line, col;
  union {
    struct
    {
      const uint8_t *name;
      size_t name_len;
      RecordEntry *entries;
      VarEntry *entry;
    } record;
    struct
    {
      VarEntry *entry;
      Scope scope;
      ProcedureEntryPoint entry_point;
      const uint8_t *name;
      size_t name_len;
      Statement *stmts;
      Parameter *params;
      Type *return_type;
    } proc;
  };
  struct Toplevel *next;
} Toplevel;


typedef struct
{
  Toplevel *toplevels;
  Scope scope;
  Scope type_scope;
  BSLAlloc *alloc;
  BSLCompileResult *result;
} AST;

#endif
