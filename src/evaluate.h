#ifndef EVALUATE_H
#define EVALUATE_H

#include "shogi.h"

// --------------------
//    評価関数
// --------------------

namespace Eval {
// --- 評価関数で使う定数 KPP(玉と任意2駒)のPに相当するenum

// BonanzaのようにKPPを求めるときに、39の地点の歩のように、
// 升×駒種に対して一意な番号が必要となる。これをBonaPiece型と呼ぶことにする。
enum BonaPiece : int16_t
{
  // f = friend(≒先手)の意味。e = enemy(≒後手)の意味

  BONA_PIECE_ZERO = 0, // 無効な駒。駒落ちのときなどは、不要な駒をここに移動させる。

  // --- 手駒
  f_hand_pawn = BONA_PIECE_ZERO + 1,
  e_hand_pawn = f_hand_pawn + 18,
  f_hand_lance = e_hand_pawn + 18,
  e_hand_lance = f_hand_lance + 4,
  f_hand_knight = e_hand_lance + 4,
  e_hand_knight = f_hand_knight + 4,
  f_hand_silver = e_hand_knight + 4,
  e_hand_silver = f_hand_silver + 4,
  f_hand_gold = e_hand_silver + 4,
  e_hand_gold = f_hand_gold + 4,
  f_hand_bishop = e_hand_gold + 4,
  e_hand_bishop = f_hand_bishop + 2,
  f_hand_rook = e_hand_bishop + 2,
  e_hand_rook = f_hand_rook + 2,
  fe_hand_end = e_hand_rook + 2,

  // Bonanzaのように番号を詰めない。
  // 理由1) 学習のときに相対PPで1段目に香がいるときがあって、それが逆変換において正しく表示するのが難しい。
  // 理由2) 縦型BitboardだとSquareからの変換に困る。

  // --- 盤上の駒
  f_pawn = fe_hand_end,
  e_pawn = f_pawn + 81,
  f_lance = e_pawn + 81,
  e_lance = f_lance + 81,
  f_knight = e_lance + 81,
  e_knight = f_knight + 81,
  f_silver = e_knight + 81,
  e_silver = f_silver + 81,
  f_gold = e_silver + 81,
  e_gold = f_gold + 81,
  f_bishop = e_gold + 81,
  e_bishop = f_bishop + 81,
  f_horse = e_bishop + 81,
  e_horse = f_horse + 81,
  f_rook = e_horse + 81,
  e_rook = f_rook + 81,
  f_dragon = e_rook + 81,
  e_dragon = f_dragon + 81,
  fe_end = e_dragon + 81,

  // 王も一意な駒番号を付与。これは2駒関係をするときに王に一意な番号が必要なための拡張
  f_king = fe_end,
  e_king = f_king + SQ_NB,
  fe_end2 = e_king + SQ_NB, // 玉も含めた末尾の番号。

  // 末尾は評価関数の性質によって異なるので、BONA_PIECE_NBを定義するわけにはいかない。
};

// BonaPieceの内容を表示する。手駒ならH,盤上の駒なら升目。例) HP3 (3枚目の手駒の歩)
std::ostream& operator<<(std::ostream& os, BonaPiece bp);

// BonaPieceを後手から見たとき(先手の39の歩を後手から見ると後手の71の歩)の番号とを
// ペアにしたものをExtBonaPiece型と呼ぶことにする。
struct ExtBonaPiece
{
  BonaPiece fb; // from black
  BonaPiece fw; // from white
};

// BonaPiece、f側だけを表示する。
inline std::ostream& operator<<(std::ostream& os, ExtBonaPiece bp) { os << bp.fb; return os; }

// KPPテーブルの盤上の駒pcに対応するBonaPieceを求めるための配列。
// 例)
// BonaPiece fb = kpp_board_index[pc].fb + sq; // 先手から見たsqにあるpcに対応するBonaPiece
// BonaPiece fw = kpp_board_index[pc].fw + sq; // 後手から見たsqにあるpcに対応するBonaPiece
const ExtBonaPiece kpp_board_index[PIECE_NB] = {
  { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
  { f_pawn, e_pawn },
  { f_lance, e_lance },
  { f_knight, e_knight },
  { f_silver, e_silver },
  { f_bishop, e_bishop },
  { f_rook, e_rook },
  { f_gold, e_gold },
  { f_king, e_king },
  { f_gold, e_gold }, // 成歩
  { f_gold, e_gold }, // 成香
  { f_gold, e_gold }, // 成桂
  { f_gold, e_gold }, // 成銀
  { f_horse, e_horse }, // 馬
  { f_dragon, e_dragon }, // 龍
  { BONA_PIECE_ZERO, BONA_PIECE_ZERO }, // 金の成りはない

  // 後手から見た場合。fとeが入れ替わる。
  { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
  { e_pawn, f_pawn },
  { e_lance, f_lance },
  { e_knight, f_knight },
  { e_silver, f_silver },
  { e_bishop, f_bishop },
  { e_rook, f_rook },
  { e_gold, f_gold },
  { e_king, f_king },
  { e_gold, f_gold }, // 成歩
  { e_gold, f_gold }, // 成香
  { e_gold, f_gold }, // 成桂
  { e_gold, f_gold }, // 成銀
  { e_horse, f_horse }, // 馬
  { e_dragon, f_dragon }, // 龍
  { BONA_PIECE_ZERO, BONA_PIECE_ZERO }, // 金の成りはない
};

// KPPの手駒テーブル
const ExtBonaPiece kpp_hand_index[COLOR_NB][KING] = {
  {
    { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
    { f_hand_pawn, e_hand_pawn },
    { f_hand_lance, e_hand_lance },
    { f_hand_knight, e_hand_knight },
    { f_hand_silver, e_hand_silver },
    { f_hand_bishop, e_hand_bishop },
    { f_hand_rook, e_hand_rook },
    { f_hand_gold, e_hand_gold },
  },
  {
    { BONA_PIECE_ZERO, BONA_PIECE_ZERO },
    { e_hand_pawn, f_hand_pawn },
    { e_hand_lance, f_hand_lance },
    { e_hand_knight, f_hand_knight },
    { e_hand_silver, f_hand_silver },
    { e_hand_bishop, f_hand_bishop },
    { e_hand_rook, f_hand_rook },
    { e_hand_gold, f_hand_gold },
  },
};

// 評価関数で用いる駒リスト。どの駒(PieceNo)がどこにあるのか(BonaPiece)を保持している構造体
struct EvalList {

  // 評価関数(FV38型)で用いる駒番号のリスト
  inline ExtBonaPiece* piece_list() const { return const_cast<ExtBonaPiece*>(pieceList); }

  // 盤上のsqの升にpiece_noのpcの駒を配置する
  void put_piece(PieceNo piece_no, Square sq, Piece pc) {
    set_piece(piece_no, BonaPiece(kpp_board_index[pc].fb + sq), BonaPiece(kpp_board_index[pc].fw + Inv(sq)));
  }

  // c側の手駒ptのi+1枚目の駒のPieceNoを設定する。(1枚目の駒のPieceNoを設定したいならi==0にして呼び出すの意味)
  void put_piece(PieceNo piece_no, Color c,Piece pt, int i){
    set_piece(piece_no,BonaPiece(kpp_hand_index[c][pt].fb + i),BonaPiece(kpp_hand_index[~c][pt].fw + i));
  }

  // あるBonaPieceに対応するPieceNoを返す。
  inline PieceNo piece_no_of(BonaPiece bp) const { return piece_no_list[bp]; }

  // pieceListを初期化する。
  // 駒落ちに対応させる時のために、未使用の駒の値はBONA_PIECE_ZEROにしておく。
  // 通常の評価関数を駒落ちの評価関数として流用できる。
  void clear() { for (auto& p : pieceList) p.fb = p.fw = BONA_PIECE_ZERO; }

protected:

  // piece_noの駒のBonaPieceがfb,fwであることを設定する。
  inline void set_piece(PieceNo piece_no, BonaPiece fb, BonaPiece fw)
  {
    ASSERT_LV3(is_ok(piece_no));
    pieceList[piece_no].fb = fb;
    pieceList[piece_no].fw = fw;
    piece_no_list[fb] = piece_no;
  }

  // 駒リスト。駒番号(PieceNo)いくつの駒がどこにあるのか(BonaPiece)を示す。FV38などで用いる。
  ExtBonaPiece pieceList[PIECE_NO_NB];

  // あるBonaPieceに対して、その駒番号(PieceNo)を保持している配列
  PieceNo piece_no_list[fe_end2];
};

// evaluateの起動時に行なう軽量な初期化はここで行なう。
inline void init() {}

// 評価関数ファイルを読み込む。
void load_eval();

// 駒割りを計算する。Position::set()から呼び出されて、以降do_move()では差分計算されるのでこの関数は呼び出されない。
Value material(const Position& pos);

// 評価関数本体
Value eval(const Position& pos);

// 駒の価値
extern int PieceValue[PIECE_NB];

// 「と」を取ったとき、評価値の変動量は手駒歩+盤面の「と」
// MovePickerとSEEの計算で用いる。
extern int PieceValueCapture[PIECE_NB];

// 駒を成ったときの成る前との価値の差。SEEで用いる。
// 後手の分はPosition::init()で先手の値をコピーして初期化している。
extern int ProDiffPieceValue[PIECE_NB];

}

#endif // #ifndef EVALUATE_H
