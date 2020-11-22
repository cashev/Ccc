#include "Ccc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  Token *tok = tokenize(argv[1]);
  Function *prog = parse(tok);

  // ASTからアセンブリを出力する
  codegen(prog);

  return 0;
}