#ifndef _BITBOARD_H_
#define _BITBOARD_H_

#include "shogi.h"

// --------------------
//     Bitboard
// --------------------

namespace Bitboards{
  void init(); // Bitboard関連のテーブル初期化のための関数
}

// Bitboard本体クラス
// SSE/AVX2専用。
struct alignas(16) Bitboard
{
  union
  {
    // 64bitずつとして扱いとき用
    uint64_t p[2];

    // SSEで取り扱い時用
    // bit0がSQ_11,bit1がSQ_12,…,bit81がSQ_99を表現する。
    // このbit位置がSquare型と対応する。
    // ただしbit63は未使用。これは、ここを余らせることで飛車の利きをpext1回で求めるためのhack。
    // Aperyを始めとするmagic bitboard派によって考案された。
    __m128i m;
  };

  // 代入等においてはSSEを使ったコピーがなされて欲しい
  Bitboard& operator = (const Bitboard& rhs) { _mm_store_si128(&this->m, rhs.m); return *this; }

  // --- ctor

  // 初期化しない。このとき中身は不定。
  Bitboard() {}

  // 代入等においてはSSEを使ったコピーがなされて欲しい
  Bitboard(const Bitboard& bb) { _mm_store_si128(&this->m, bb.m); }

  // p[0],p[1]の値を直接指定しての初期化。(Bitboard定数の初期化のときのみ用いる)
  Bitboard(uint64_t p0, uint64_t p1) { p[0] = p0; p[1] = p1; }

  // sqの升が1のBitboardとして初期化する。
  Bitboard(Square sq);

  // 値を直接代入する。
  void set(uint64_t p0, uint64_t p1) { p[0] = p0; p[1] = p1; }

  // --- property

  // Stockfishのソースとの互換性がよくなるようにboolへの暗黙の型変換書いておく。
  operator bool() const { return !(_mm_testz_si128(m, _mm_set1_epi8(static_cast<char>(0xffu)))); }

  // p[0]とp[1]をorしたものを返す。toU()相当。
  uint64_t merge() const { return p[0] | p[1]; }

  // p[0]とp[1]とで and したときに被覆しているbitがあるか。
  // merge()したあとにpext()を使うときなどに被覆していないことを前提とする場合にそのassertを書くときに使う。
  bool cross_over() const { return p[0] & p[1]; }

  // 指定した升(Square)が Bitboard のどちらの u64 変数の要素に属するか。
  // 本ソースコードのように縦型Bitboardにおいては、香の利きを求めるのにBitboardの
  // 片側のp[x]を調べるだけで済むので、ある升がどちらに属するかがわかれば香の利きは
  // そちらを調べるだけで良いというAperyのアイデア。
  constexpr static int part(const Square sq) { return static_cast<int>(SQ_79 < sq); }

  // --- operator

  // 下位bitから1bit拾ってそのbit位置を返す。
  // 絶対に1bitはnon zeroと仮定
  // while(to = bb.pop())
  //  make_move(from,to);
  // のように用いる。
  Square pop() { return (p[0] != 0) ? Square(pop_lsb(p[0])) : Square(pop_lsb(p[1]) + 63); }

  // このBitboardの値を変えないpop()
  Square pop_c() const { return (p[0] != 0) ? Square(lsb(p[0])) : Square(lsb(p[1]) + 63); }

  // pop()をp[0],p[1]に分けて片側ずつする用
  Square pop_from_p0() { ASSERT_LV3(p[0] != 0);  return Square(pop_lsb(p[0])); }
  Square pop_from_p1() { ASSERT_LV3(p[1] != 0);  return Square(pop_lsb(p[1]) + 63); }

  // 1のbitを数えて返す。
  int pop_count() const { return (int)(POPCNT64(p[0]) + POPCNT64(p[1])); }

  // 代入型演算子

  Bitboard& operator |= (const Bitboard& b1) { this->m = _mm_or_si128( m, b1.m); return *this; }
  Bitboard& operator &= (const Bitboard& b1) { this->m = _mm_and_si128(m, b1.m); return *this; }
  Bitboard& operator ^= (const Bitboard& b1) { this->m = _mm_xor_si128(m, b1.m); return *this; }
  Bitboard& operator += (const Bitboard& b1) { this->m = _mm_add_epi64(m, b1.m); return *this; }
  Bitboard& operator -= (const Bitboard& b1) { this->m = _mm_sub_epi64(m, b1.m); return *this; }

  // 左シフト(縦型Bitboardでは左1回シフトで1段下の升に移動する)
  // ※　シフト演算子は歩の利きを求めるためだけに使う。
  Bitboard& operator <<= (int shift) { ASSERT_LV3(shift == 1); m = _mm_slli_epi64(m, shift); return *this; }

  // 右シフト(縦型Bitboardでは右1回シフトで1段上の升に移動する)
  Bitboard& operator >>= (int shift) { ASSERT_LV3(shift == 1); m = _mm_srli_epi64(m, shift); return *this; }

  // 比較演算子

  bool operator == (const Bitboard& rhs) const {
    return (_mm_testc_si128(_mm_cmpeq_epi8(this->m, rhs.m), _mm_set1_epi8(static_cast<char>(0xffu))) ? true : false);
  }
  bool operator != (const Bitboard& rhs) const { return !(*this == rhs); }

  // 2項演算子

  Bitboard operator & (const Bitboard& rhs) const { return Bitboard(*this) &= rhs; }
  Bitboard operator | (const Bitboard& rhs) const { return Bitboard(*this) |= rhs; }
  Bitboard operator ^ (const Bitboard& rhs) const { return Bitboard(*this) ^= rhs; }
  Bitboard operator + (const Bitboard& rhs) const { return Bitboard(*this) += rhs; }
  Bitboard operator << (const int i) const { return Bitboard(*this) <<= i; }
  Bitboard operator >> (const int i) const { return Bitboard(*this) >>= i; }

  // range-forで回せるようにするためのhack(少し遅いので速度が要求されるところでは使わないこと)
  Square operator*() { return pop(); }
  void operator++() {}
};

// sqの升が1であるbitboard
extern Bitboard SquareBB[SQ_NB_PLUS1];
inline Bitboard::Bitboard(Square sq) { *this = SquareBB[sq]; }

// 全升が1であるBitboard
// p[0]の63bit目は0
const Bitboard ALL_BB = Bitboard(UINT64_C(0x7FFFFFFFFFFFFFFF), UINT64_C(0x3FFFF));

// 全升が0であるBitboard
const Bitboard ZERO_BB = Bitboard(0, 0);

// Square型との演算子
inline Bitboard operator|(const Bitboard& b, Square s) { return b | SquareBB[s]; }
inline Bitboard operator&(const Bitboard& b, Square s) { return b & SquareBB[s]; }
inline Bitboard operator^(const Bitboard& b, Square s) { return b ^ SquareBB[s]; }

// 単項演算子
// →　NOTで書くと、使っていないbit(p[0]のbit63)がおかしくなるのでALL_BBでxorしないといけない。
inline Bitboard operator ~ (const Bitboard& a) { Bitboard t;  t.m = _mm_xor_si128(a.m, ALL_BB.m); return t; }

// range-forで回せるようにするためのhack(少し遅いので速度が要求されるところでは使わないこと)
inline const Bitboard begin(const Bitboard& b) { return b; }
inline const Bitboard end(const Bitboard& b) { return ZERO_BB; }

// Bitboardの1の升を'*'、0の升を'.'として表示する。デバッグ用。
std::ostream& operator<<(std::ostream& os, const Bitboard& board);

// --------------------
//     Bitboard定数
// --------------------

// 各筋を表現するBitboard定数
const Bitboard FILE1_BB = Bitboard(UINT64_C(0x1ff) << (9 * 0), 0);
const Bitboard FILE2_BB = Bitboard(UINT64_C(0x1ff) << (9 * 1), 0);
const Bitboard FILE3_BB = Bitboard(UINT64_C(0x1ff) << (9 * 2), 0);
const Bitboard FILE4_BB = Bitboard(UINT64_C(0x1ff) << (9 * 3), 0);
const Bitboard FILE5_BB = Bitboard(UINT64_C(0x1ff) << (9 * 4), 0);
const Bitboard FILE6_BB = Bitboard(UINT64_C(0x1ff) << (9 * 5), 0);
const Bitboard FILE7_BB = Bitboard(UINT64_C(0x1ff) << (9 * 6), 0);
const Bitboard FILE8_BB = Bitboard(0, 0x1ff << (9 * 0));
const Bitboard FILE9_BB = Bitboard(0, 0x1ff << (9 * 1));

// 各段を表現するBitboard定数
const Bitboard RANK1_BB = Bitboard(UINT64_C(0x40201008040201) << 0, 0x201 << 0);
const Bitboard RANK2_BB = Bitboard(UINT64_C(0x40201008040201) << 1, 0x201 << 1);
const Bitboard RANK3_BB = Bitboard(UINT64_C(0x40201008040201) << 2, 0x201 << 2);
const Bitboard RANK4_BB = Bitboard(UINT64_C(0x40201008040201) << 3, 0x201 << 3);
const Bitboard RANK5_BB = Bitboard(UINT64_C(0x40201008040201) << 4, 0x201 << 4);
const Bitboard RANK6_BB = Bitboard(UINT64_C(0x40201008040201) << 5, 0x201 << 5);
const Bitboard RANK7_BB = Bitboard(UINT64_C(0x40201008040201) << 6, 0x201 << 6);
const Bitboard RANK8_BB = Bitboard(UINT64_C(0x40201008040201) << 7, 0x201 << 7);
const Bitboard RANK9_BB = Bitboard(UINT64_C(0x40201008040201) << 8, 0x201 << 8);

// 各筋を表現するBitboard配列
const Bitboard FILE_BB[FILE_NB] = { FILE1_BB,FILE2_BB,FILE3_BB,FILE4_BB,FILE5_BB,FILE6_BB,FILE7_BB,FILE8_BB,FILE9_BB };

// 各段を表現するBitboard配列
const Bitboard RANK_BB[RANK_NB] = { RANK1_BB,RANK2_BB,RANK3_BB,RANK4_BB,RANK5_BB,RANK6_BB,RANK7_BB,RANK8_BB,RANK9_BB };

// InFrontBBの定義)
//    c側の香の利き = 飛車の利き & InFrontBB[c][rank_of(sq)]
//
// すなわち、
// color == BLACKのとき、n段目よりWHITE側(1からn-1段目)を表現するBitboard。
// color == WHITEのとき、n段目よりBLACK側(n+1から9段目)を表現するBitboard。
// このアイデアはAperyのもの。
const Bitboard InFrontBB[COLOR_NB][RANK_NB] = {
  { ZERO_BB,RANK1_BB, RANK1_BB | RANK2_BB , RANK1_BB | RANK2_BB | RANK3_BB , RANK1_BB | RANK2_BB | RANK3_BB | RANK4_BB,
  ~(RANK9_BB | RANK8_BB | RANK7_BB | RANK6_BB) , ~(RANK9_BB | RANK8_BB | RANK7_BB),~(RANK9_BB | RANK8_BB),~RANK9_BB },
  { ~RANK1_BB , ~(RANK1_BB | RANK2_BB) , ~(RANK1_BB | RANK2_BB | RANK3_BB),~(RANK1_BB | RANK2_BB | RANK3_BB | RANK4_BB),
  RANK9_BB | RANK8_BB | RANK7_BB | RANK6_BB , RANK9_BB | RANK8_BB | RANK7_BB , RANK9_BB | RANK8_BB , RANK9_BB , ZERO_BB }
};

// 先手から見て1段目からr段目までを表現するBB(US==WHITEなら、9段目から数える)
inline const Bitboard rank1_n_bb(const Color US, const Rank r) { ASSERT_LV2(is_ok(r));  return InFrontBB[US][(US == BLACK ? r + 1 : 7 - r)]; }

// 敵陣を表現するBitboard。
inline const Bitboard enemy_field(const Color US) { return rank1_n_bb(US, RANK_3); }

// 歩が打てる筋を得るためのBitboard mask
extern Bitboard PAWN_DROP_MASK_BB[0x200][COLOR_NB];

// 2升に挟まれている升を返すためのテーブル(その2升は含まない)
extern Bitboard BetweenBB[SQ_NB_PLUS1][SQ_NB_PLUS1];

// 2升に挟まれている升を表すBitboardを返す。sq1とsq2が縦横斜めの関係にないときはZERO_BBが返る。
inline const Bitboard between_bb(Square sq1, Square sq2) { return BetweenBB[sq1][sq2]; }

// 2升を通過する直線を返すためのテーブル
extern Bitboard LineBB[SQ_NB_PLUS1][SQ_NB_PLUS1];

// 2升を通過する直線を返すためのBitboardを返す。sq1とsq2が縦横斜めの関係にないときはZERO_BBが返る。
inline const Bitboard line_bb(Square sq1, Square sq2) { return LineBB[sq1][sq2]; }

// sqの升にいる敵玉に王手となるc側の駒ptの候補を得るテーブル。第2添字は(pr-1)を渡して使う。
extern Bitboard CheckCandidateBB[SQ_NB_PLUS1][HDK][COLOR_NB];

// sqの升にいる敵玉に王手となるus側の駒ptの候補を得る
// pr == ROOKは無条件全域なので代わりにHORSEで王手になる領域を返す。
// pr == KINGはsqの24近傍を返す。(ただしこれは王手生成では使わない)
inline const Bitboard check_candidate_bb(Color us, Piece pr, Square sq) { ASSERT_LV3(PAWN<= pr && pr <= HDK); return CheckCandidateBB[sq][pr - 1][us]; }

// ある升の24近傍のBitboardを返す。
// inline const Bitboard around24_bb(Square sq) { return check_candidate_bb(BLACK, KING, sq); }
// → あとで使うかも。

// --------------------
//  Bitboard用の駒定数
// --------------------

// Bitboardの配列用の定数
// StepEffect[]の添字で使う。
enum PieceTypeBitboard
{
  PIECE_TYPE_BITBOARD_PAWN,
  PIECE_TYPE_BITBOARD_LANCE,
  PIECE_TYPE_BITBOARD_KNIGHT,
  PIECE_TYPE_BITBOARD_SILVER,
  PIECE_TYPE_BITBOARD_BISHOP,
  PIECE_TYPE_BITBOARD_ROOK,
  PIECE_TYPE_BITBOARD_GOLD,
  PIECE_TYPE_BITBOARD_HDK, // Horse , Dragon , King

  PIECE_TYPE_BITBOARD_NB = 8, // ビットボードを示すときにのみ使う定数

  // 以下、StepEffectで使う特殊な定数
  PIECE_TYPE_BITBOARD_QUEEN = 8,  // 馬+龍
  PIECE_TYPE_BITBOARD_CROSS00 = 9,   // 十字方向に1升
  PIECE_TYPE_BITBOARD_CROSS45 = 10,  // ×字方向に1升

  PIECE_TYPE_BITBOARD_NB2 = 16, // StepEffectに使う特殊な定数
};

// --------------------
// 利きのためのテーブル
// --------------------

// 利きのためのライブラリ
// Bitboardを用いるとソースコードが長くなるが、ソースコードのわかりやすさ、速度において現実的なのであえて使う。

// 近接駒の利き
// 3番目の添字がPIECE_TYPE_BITBOARD_LANCE,PIECE_TYPE_BITBOARD_BISHOP,PIECE_TYPE_BITBOARD_ROOK
// のときは、盤上の駒の状態を無視した(盤上に駒がないものとする)香・角・飛の利き。
// また、PIECE_TYPE_BITBOARD_QUEEN,PIECE_TYPE_BITBOARD_CROSS00,PIECE_TYPE_BITBOARD_CROSS45
// は、馬+龍,十字方向に1升,×字方向に1升となる。
extern Bitboard StepEffectsBB[SQ_NB_PLUS1][COLOR_NB][PIECE_TYPE_BITBOARD_NB2];

// --- 香の利き
extern Bitboard LanceEffect[COLOR_NB][SQ_NB_PLUS1][128];

// 指定した位置の属する file の bit を shift し、
// index を求める為に使用する。(from Apery)
const int Slide[SQ_NB_PLUS1] = {
  1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 ,
  10, 10, 10, 10, 10, 10, 10, 10, 10,
  19, 19, 19, 19, 19, 19, 19, 19, 19,
  28, 28, 28, 28, 28, 28, 28, 28, 28,
  37, 37, 37, 37, 37, 37, 37, 37, 37,
  46, 46, 46, 46, 46, 46, 46, 46, 46,
  55, 55, 55, 55, 55, 55, 55, 55, 55,
  1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 ,
  10, 10, 10, 10, 10, 10, 10, 10, 10,
  0 , // SQ_NB用
};

// --- 角の利き
extern Bitboard BishopEffect[20224+1];
extern Bitboard BishopEffectMask[SQ_NB_PLUS1];
extern int BishopEffectIndex[SQ_NB_PLUS1];

// --- 飛車の利き
extern Bitboard RookEffect[495616+1];
extern Bitboard RookEffectMask[SQ_NB_PLUS1];
extern int RookEffectIndex[SQ_NB_PLUS1];

// Haswellのpext()を呼び出す。occupied = occupied bitboard , mask = 利きの算出に絡む升が1のbitboard
// この関数で戻ってきた値をもとに利きテーブルを参照して、遠方駒の利きを得る。
inline uint64_t occupiedToIndex(const Bitboard& occupied, const Bitboard& mask) { return PEXT64(occupied.merge(), mask.merge()); }

// --------------------
//   大駒・小駒の利き
// --------------------

// --- 近接駒

// 王の利き
inline Bitboard kingEffect(const Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_HDK]; }

// 歩の利き
inline Bitboard pawnEffect(const Color color, const Square sq) { return StepEffectsBB[sq][color][PIECE_TYPE_BITBOARD_PAWN]; }

// 桂の利き
inline Bitboard knightEffect(const Color color, const Square sq) { return StepEffectsBB[sq][color][PIECE_TYPE_BITBOARD_KNIGHT]; }

// 銀の利き
inline Bitboard silverEffect(const Color color, const Square sq) { return StepEffectsBB[sq][color][PIECE_TYPE_BITBOARD_SILVER]; }

// 金の利き
inline Bitboard goldEffect(const Color color, const Square sq) { return StepEffectsBB[sq][color][PIECE_TYPE_BITBOARD_GOLD]; }

// --- 遠方仮想駒(盤上には駒がないものとして求める利き)

// 盤上の駒を無視するQueenの動き。
inline Bitboard queenStepEffect(const Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_QUEEN]; }

// 十字の利き 利き長さ=1升分。
inline Bitboard cross00StepEffect(Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_CROSS00]; }

// 斜め十字の利き 利き長さ=1升分。
inline Bitboard cross45StepEffect(Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_CROSS45]; }

// 盤上の駒を考慮しない香の利き
inline Bitboard lanceStepEffect(Color c, Square sq) { return StepEffectsBB[sq][c][PIECE_TYPE_BITBOARD_LANCE];}

// 盤上の駒を考慮しない角の利き
inline Bitboard bishopStepEffect(Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_BISHOP]; }

// 盤上の駒を考慮しない飛車の利き
inline Bitboard rookStepEffect(Square sq) { return StepEffectsBB[sq][BLACK][PIECE_TYPE_BITBOARD_ROOK];}

// --- 遠方駒(盤上の駒の状態を考慮しながら利きを求める)

// 香 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard lanceEffect(const Color c,const Square sq, const Bitboard& occupied) {
  const int index = (occupied.p[Bitboard::part(sq)] >> Slide[sq]) & 127;
  return LanceEffect[c][sq][index];
}

// 角 : occupied bitboardを考慮しながら角の利きを求める
inline Bitboard bishopEffect(const Square sq, const Bitboard& occupied) {
  const Bitboard block(occupied & BishopEffectMask[sq]);
  return BishopEffect[BishopEffectIndex[sq] + occupiedToIndex(block, BishopEffectMask[sq])];
}

// 馬 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard horseEffect(const Square sq, const Bitboard& occupied) { return bishopEffect(sq, occupied) | kingEffect(sq); }

// 飛 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard rookEffect(const Square sq, const Bitboard& occupied)
{
  const Bitboard block(occupied & RookEffectMask[sq]);
  return RookEffect[RookEffectIndex[sq] + occupiedToIndex(block, RookEffectMask[sq])];
}

// 龍 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard dragonEffect(const Square sq, const Bitboard& occupied){ return rookEffect(sq, occupied) | kingEffect(sq); }

// 上下にしか利かない飛車の利き
inline Bitboard rookEffectFile(const Square sq, const Bitboard& occupied) {
	const int index = (occupied.p[Bitboard::part(sq)] >> Slide[sq]) & 127;
	return LanceEffect[BLACK][sq][index] | LanceEffect[WHITE][sq][index];
}

// --------------------
//   汎用性のある利き
// --------------------

// 盤上sqに駒pc(先後の区別あり)を置いたときの利き。
Bitboard effects_from(Piece pc, Square sq, const Bitboard& occ);

// --------------------
//   Bitboard tools
// --------------------

// 2bit以上あるかどうかを判定する。縦横斜め方向に並んだ駒が2枚以上であるかを判定する。この関係にないと駄目。
// この関係にある場合、Bitboard::merge()によって被覆しないことがBitboardのレイアウトから保証されている。
inline bool more_than_one(const Bitboard& bb) { ASSERT_LV2(!bb.cross_over()); return POPCNT64(bb.merge()) > 1; }


#endif // #ifndef _BITBOARD_H_
