#include <sstream>
#include <iostream>

#include "shogi.h"
#include "position.h"
#include "search.h"
#include "thread.h"

// ----------------------------------------
// operator<<(std::ostream& os,...)とpretty() 
// ----------------------------------------

std::string pretty(File f) { return pretty_jp ? std::string("１２３４５６７８９").substr((int32_t)f * 2, 2) : std::to_string((int32_t)f + 1); }
std::string pretty(Rank r) { return pretty_jp ? std::string("一二三四五六七八九").substr((int32_t)r * 2, 2) : std::to_string((int32_t)r + 1); }

std::string pretty(Move m)
{
  if (is_drop(m))
    return (pretty(move_to(m)) + pretty2(Piece(move_from(m))) + (pretty_jp ? "打" : "*"));
  else
    return pretty(move_from(m)) + pretty(move_to(m)) + (is_promote(m) ? (pretty_jp ? "成" : "+") : "");
}

std::string pretty(Move m, Piece movedPieceType)
{
  if (is_drop(m))
    return (pretty(move_to(m)) + pretty2(movedPieceType) + (pretty_jp ? "打" : "*"));
  else
    return pretty(move_to(m)) + pretty2(movedPieceType) + (is_promote(m) ? (pretty_jp ? "成" : "+") : "") + "["+ pretty(move_from(m))+"]";
}

std::ostream& operator<<(std::ostream& os, Color c) { os << ((c == BLACK) ? (pretty_jp ? "先手" : "BLACK") : (pretty_jp ? "後手" : "WHITE")); return os; }

std::ostream& operator<<(std::ostream& os, Piece pc)
{
  auto s = usi_piece(pc);
  if (s[1] == ' ') s.resize(1); // 手動trim
  os << s;
  return os;
}

std::ostream& operator<<(std::ostream& os, Hand hand)
{
  for (Piece pr = PAWN; pr < PIECE_HAND_NB; ++pr)
  {
    int c = hand_count(hand, pr);
    // 0枚ではないなら出力。
    if (c != 0)
    {
      // 1枚なら枚数は出力しない。2枚以上なら枚数を最初に出力
      // PRETTY_JPが指定されているときは、枚数は後ろに表示。
      const std::string cs = (c != 1) ? std::to_string(c) : "";
      std::cout << (pretty_jp ? "" : cs) << pretty(pr) << (pretty_jp ? cs : "");
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, HandKind hk)
{
  for (Piece pc = PAWN; pc < PIECE_HAND_NB; ++pc)
    if (hand_exists(hk, pc))
      std::cout << pretty(pc);
  return os;
}

std::ostream& operator<<(std::ostream& os, Move m)
{
  if (!is_ok(m))
  {
    os <<((m == MOVE_RESIGN) ? "resign" :
          (m == MOVE_NULL  ) ? "null" :
          (m == MOVE_NONE  ) ? "none" :
          "");
  }
  else if (is_drop(m))
  {
    os << Piece(move_from(m));
    os << '*';
    os << move_to(m);
  }
  else {
    os << move_from(m);
    os << move_to(m);
    if (is_promote(m))
      os << '+';
  }
  return os;
}

// ----------------------------------------

int main(int argc, char* argv[])
{
  // --- 全体的な初期化
  USI::init(Options);
  Bitboards::init();
  Position::init();
  Search::init();
  Threads.init();
  Eval::init(); // 簡単な初期化のみで評価関数の読み込みはisreadyに応じて行なう。

  // USIコマンドの応答部
  USI::loop();

  // 生成して、待機させていたスレッドの停止
  Threads.exit();

  return 0;
}
