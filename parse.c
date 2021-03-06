#include "Ccc.h"

Obj *locals;
Token *token;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
Obj *find_lvar(Token *tok) {
  for (Obj *var = locals; var; var = var->next)
    if (strlen(var->name) == tok->len && !memcmp(tok->loc, var->name, var->len))
      return var;
  return NULL;
}

char *get_ident(Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected an identifier");
  return strndup(tok->loc, tok->len);
}

// Tokenが期待している記号のときには、Tokenを1つ読み進める。
// それ以外の場合にはエラーを報告する。
Token *skip(Token *tok, char *op) {
  if (!equal(tok, op)) {
    error_tok(tok, "expected '%s'", op);
  }
  return token->next;
}

bool consume(Token *tok, char *str) {
  if(equal(tok, str)) {
    token = tok->next;
    return true;
  }
  return false;
}

bool at_eof() { return token->kind == TK_EOF; }

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

Node *new_num_node(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *new_var_node(Obj *var) {
  Node *node = new_node(ND_VAR);
  node->var = var;
  return node;
}

Obj *new_lvar(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  var->next = locals;
  locals = var;
  return var;
}

Node *declaration();
Node *stmt();
Node *compound_stmt();
Node *expr_stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// declspec = "int"
Type *declspec() {
  token = skip(token, "int");
  return ty_int;
}

// declarator = "*"* ident
Type *declarator(Type *ty) {
  while (consume(token, "*")) {
    ty = pointer_to(ty);
  }
  if (token->kind != TK_IDENT) {
    error_tok(token, "expected a variable name");
  }

  ty->name = token;
  token = token->next;
  return ty;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
Node *declaration() {
  Type *basety = declspec();

  Node head = {};
  Node *cur = &head;
  int i = 0;

  while (!equal(token, ";")) {
    if (i++ > 0) {
      token = skip(token, ",");
    }

    Type *ty = declarator(basety);
    Obj *var = new_lvar(get_ident(ty->name), ty);
    // Obj *var = new_lvar(get_ident(token), ty);
    // Obj *var = new_lvar(token->loc, ty);
    var->len = token->len;

    if (!equal(token, "=")) {
      continue;
    }
    token = skip(token, "=");

    Node *lhs = new_var_node(var);
    Node *rhs = assign();
    Node *node = new_binary(ND_ASSIGN, lhs, rhs);
    cur->next = new_unary(ND_EXPR_STMT, node);
    cur = cur->next;
  }

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;
  token = token->next;
  return node;
}

// stmt = return" expr ";"
//      | "{" compound-stmt
//      | "if" "(" expr ")" stmt ( "else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr-stmt
Node *stmt() {
  if (equal(token, "return")) {
    token = token->next;
    Node *node = new_node(ND_RETURN);
    node->lhs = expr();

    token = skip(token, ";");
    return node;
  }

  if (equal(token, "if")) {
    token = token->next;
    Node *node = new_node(ND_IF);
    token = skip(token, "(");
    node->cond = expr();
    token = skip(token, ")");
    node->then = stmt();
    if (equal(token, "else")) {
      token = token->next;
      node->els = stmt();
    }
    return node;
  }

  if (equal(token, "while")) {
    token = token->next;
    Node *node = new_node(ND_WHILE);
    token = skip(token, "(");
    node->cond = expr();
    token = skip(token, ")");
    node->then = stmt();
    return node;
  }

  if (equal(token, "for")) {
    token = token->next;
    Node *node = new_node(ND_FOR);
    token = skip(token, "(");
    if (!equal(token, ";")) {
      node->init = expr();
    }
    token = skip(token, ";");
    if (!equal(token, ";")) {
      node->cond = expr();
    }
    token = skip(token, ";");
    if (!equal(token, ")")) {
      node->inc = expr();
    }
    token = skip(token, ")");

    node->then = stmt();
    return node;
  }

  if (equal(token, "{")) {
    token = token->next;
    return compound_stmt();
  }

  return expr_stmt();
}

// compound-stmt = (declaration | stmt)* "}"
Node *compound_stmt() {
  Node head = {};
  Node *cur = &head;
  while (!equal(token, "}")) {
    if (equal(token, "int")) {
      cur->next = declaration();
      cur = cur->next;
    } else {
      cur->next = stmt();
      cur = cur->next;
    }
  }
  token = skip(token, "}");

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
}

// expr-stmt = expr? ";"
Node *expr_stmt() {
  if (equal(token, ";")) {
    token = token->next;
    return new_node(ND_BLOCK);
  }

  Node *node = new_unary(ND_EXPR_STMT, expr());
  token = skip(token, ";");
  return node;
}

// expr = assign
Node *expr() { return assign(); }

// assign = equality ("=" assign)?
Node *assign() {
  Node *node = equality();
  if (equal(token, "=")) {
    token = token->next;
    node = new_binary(ND_ASSIGN, node, assign());
  }
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (equal(token, "==")) {
      token = token->next;
      node = new_binary(ND_EQ, node, relational());
      continue;
    }
    if (equal(token, "!=")) {
      token = token->next;
      node = new_binary(ND_NE, node, relational());
      continue;
    }

    return node;
  }
}

// relational = add ("<" add | "<=" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (equal(token, "<")) {
      token = token->next;
      node = new_binary(ND_LT, node, add());
      continue;
    }
    if (equal(token, "<=")) {
      token = token->next;
      node = new_binary(ND_LE, node, add());
      continue;
    }
    if (equal(token, ">")) {
      token = token->next;
      node = new_binary(ND_LT, add(), node);
      continue;
    }
    if (equal(token, ">=")) {
      token = token->next;
      node = new_binary(ND_LE, add(), node);
      continue;
    }

    return node;
  }
}

// '+'は数値だけでなくポインタの演算にも使われる.
// p+nはポインタに整数値 nを足すのではなく、sizeof(*p)*nを足す.
Node *new_add(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  // num + num
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_ADD, lhs, rhs);
  }

  // ptr + prtは許容していない
  if (lhs->ty->base && rhs->ty->base) {
    error_tok(token, "invalid operands");
  }

  // num + ptr は ptr + num へスワップする
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // ptr + num
  rhs = new_binary(ND_MUL, rhs, new_num_node(8));
  return new_binary(ND_ADD, lhs, rhs);
}

Node *new_sub(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  // num - num
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_SUB, lhs, rhs);
  }

  // ptr - num
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_num_node(8));
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs);
    node->ty = lhs->ty;
    return node;
  }

  // ptr - ptr
  // ポインタ同士の減算は、間に要素がいくつかあるか計算する
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs);
    node->ty = ty_int;
    return new_binary(ND_DIV, node, new_num_node(8));
  }

  error_tok(token, "invalid operands");
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (equal(token, "+")) {
      token = token->next;
      node = new_add(node, mul());
      continue;
    }
    if (equal(token, "-")) {
      token = token->next;
      node = new_sub(node, mul());
      continue;
    }

    return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (equal(token, "*")) {
      token = token->next;
      node = new_binary(ND_MUL, node, unary());
      continue;
    }
    if (equal(token, "/")) {
      token = token->next;
      node = new_binary(ND_DIV, node, unary());
      continue;
    }

    return node;
  }
}

// unary = ("+" | "-" | "*" | "&") unary
//       | primary
Node *unary() {
  if (equal(token, "+")) {
    token = token->next;
    return unary();
  }

  if (equal(token, "-")) {
    token = token->next;
    return new_unary(ND_NEG, unary());
  }

  if (equal(token, "&")) {
    token = token->next;
    return new_unary(ND_ADDR, unary());
  }

  if (equal(token, "*")) {
    token = token->next;
    return new_unary(ND_DEREF, unary());
  }

  return primary();
}

// primary = num | ident args? | "(" expr ")"
// args = "(" ")"
Node *primary() {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (equal(token, "(")) {
    token = token->next;
    Node *node = expr();
    token = skip(token, ")");
    return node;
  }

  if (token->kind == TK_IDENT) {
    // 関数呼び出し
    if (equal(token->next, "(")) {
      Node *node = new_node(ND_FUNCALL);
      node->funcname = strndup(token->loc, token->len);
      token = token->next;
      token = skip(token, "(");
      token = skip(token, ")");
      return node;
    }

    // 変数
    Obj *var = find_lvar(token);
    if (!var) {
      error_tok(token, "undefined variable");
    }
    token = token->next;
    return new_var_node(var);
  }

  // 数値
  if (token->kind == TK_NUM) {
    Node *node = new_num_node(token->val);
    token = token->next;
    return node;
  }

  error_tok(token, "expected an expression");
}

// program = stmt*
Function *parse(Token *tok) {
  token = tok;
  token = skip(token, "{");

  Function *prog = calloc(1, sizeof(Function));
  prog->body = compound_stmt();
  prog->locals = locals;
  return prog;
}
