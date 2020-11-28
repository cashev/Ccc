#include "Ccc.h"

char *user_input;
Token *token;

void verror_at(char *loc, char *fmt, va_list ap) {
  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(tok->loc, fmt, ap);
}

bool equal(Token *tok, char *op) {
  return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

bool startwith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

int is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

int is_alnum(char c) { return is_alpha(c) || ('0' <= c && c <= '9'); }

int read_punct(char *p) {
  if (startwith(p, "==") || startwith(p, "!=") || startwith(p, "<=") ||
      startwith(p, ">="))
    return 2;

  return ispunct(*p) ? 1 : 0;
}

bool is_keyword(Token *tok) {
  char *kw[] = {"return", "if", "else", "for", "while"};

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return false;
}

void convert_keywords(Token *tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next)
    if (is_keyword(t))
      t->kind = TK_RESERVED;
}

// 新しいTokenを作成する
Token *new_token(TokenKind kind, char *start, char *end) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = start;
  tok->len = end - start;
  return tok;
}

// 入力文字列をトークナイズしてTokenを返す
Token *tokenize(char *p) {
  user_input = p;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Identifier or keyword
    if (is_alpha(*p)) {
      char *start = p;
      do {
        p++;
      } while (is_alnum(*p));
      cur->next = new_token(TK_IDENT, start, p);
      cur = cur->next;
      continue;
    }

    // Numeric literal
    if (isdigit(*p)) {
      cur->next = new_token(TK_NUM, p, p);
      cur = cur->next;
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    // Punctuator (区切字)
    int punct_len = read_punct(p);
    if (punct_len) {
      cur->next = new_token(TK_RESERVED, p, p + punct_len);
      cur = cur->next;
      p += cur->len;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  cur->next = new_token(TK_EOF, p, p);
  convert_keywords(head.next);
  return head.next;
}