#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

//
// tokenize.c
//

// トークンの種類
typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_NUM,      // 整数トークン
  TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// Token
typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *loc;      // トークンの位置
  int len;        // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *tokenize(char *p);

//
// parse.c
//

typedef struct Obj Obj;

// ローカル変数の型
struct Obj {
  Obj *next; // 次の変数かNULL
  char *name; // 変数の名前
  Type *ty;   // 変数の型
  int len;    // 名前の長さ
  int offset; // RBPからのオフセット
};

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_ASSIGN,    // =
  ND_ADDR,      // unary &
  ND_DEREF,     // unary *
  ND_RETURN,    // "return"
  ND_IF,        // "if"
  ND_WHILE,     // "while"
  ND_FOR,       // "for"
  ND_BLOCK,     // { ... }
  ND_EXPR_STMT, // Expression statement
  ND_VAR,      // ローカル変数
  ND_NUM,       // 整数
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型

  Type *ty; // Type 例: intなど

  Node *next; // 次のノード
  Node *lhs;  // 左辺
  Node *rhs;  // 右辺

  // "if", "for"
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Block
  Node *body;

  Obj *var; // kindがND_LVARの場合のみ使う
  int val;   // kindがND_NUMの場合のみ使う
};

// Function
typedef struct Function Function;
struct Function {
  Node *body;
  Obj *locals;
  int stack_size;
};

//
// type.c
//

typedef enum {
  TY_INT,
  TY_PTR,
} TypeKind;

struct Type {
  TypeKind kind;

  // Pointer
  Type *base;

  // Declaration 宣言
  Token *name;
};

extern Type *ty_int;

bool is_integer(Type *ty);
Type *pointer_to(Type *base);
void add_type(Node *node);


Function *parse(Token *tok);

//
// codegen.c
//

void codegen(Function *prog);
