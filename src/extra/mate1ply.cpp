#include "../shogi.h"
#include "../position.h"

// 超高速1手詰め判定ライブラリ
// cf. 新規節点で固定深さの探索を併用するdf-pnアルゴリズム gpw05.pdf

#ifdef MATE_1PLY

namespace {

  // 方角を表す。遠方駒の利きや、玉から見た方角を表すのに用いる。
  // bit0..左上、bit1..上、bit2..右上、bit3..左、bit4..右、bit5..左下、bit6..下、bit7..右下
  enum Directions : uint8_t {
    DIRECTIONS_LU = 1 , DIRECTIONS_U  = 2  , DIRECTIONS_RU = 4  , DIRECTIONS_L = 8 ,
    DIRECTIONS_R=16   , DIRECTIONS_LD = 32 , DIRECTIONS_D  = 64 , DIRECTIONS_RD = 128 };
  const int DIRECTIONS_NB = 256; // ↑のenumで表現できない範囲なので出しておく。

  // 方角 (Directionsをpopしたもの)
  enum Direct { DIRECT_NB = 8 };
  inline bool is_ok(Direct d) { return 0 <= d && d < DIRECT_NB; }
  inline Direct pop_directions(uint32_t& d) { return (Direct)pop_lsb(d); }

  // DirectをSquare型の差分値で表現したもの
  const Square DirectToDelta_[DIRECT_NB] = { DELTA_SW,DELTA_S,DELTA_SE,DELTA_W,DELTA_E,DELTA_NW,DELTA_N,DELTA_NE };
  inline Square DirectToDelta(Direct d) { ASSERT_LV3(is_ok(d));  return DirectToDelta_[d]; }

  // 1手詰め判定高速化テーブル
  struct alignas(8) MateInfo
  {
    union {
      // この形において詰ませるのに必要な駒種
      uint8_t/*HandKind*/ hand_kind;

      // 玉から見てどの方向に打てば詰むのかの情報(要素[0]がもったいないので、unionにしてこの要素はhand_kindと共用にする。
      Directions directions[PIECE_RAW_NB];
    };
  };

  // 添字として
  //  bit  0.. 8 : (1) 駒を打つ候補の升(受け方の利きがなく、攻め方の利きがある升) 壁の扱いは0でも1でも問題ない。
  //  bit  9..15 : (2) 王が移動可能な升(攻め方の利きがなく、受け方の駒もない) 壁は0(玉はそこに移動できないので)
  // を与えて、そのときに詰ませられる候補の駒種(HandKind)を返すテーブル。
  // ただし、歩は打ち歩詰めになるので、移動させることが前提。桂は打つ先の升に敵の利きがないことが前提。
  MateInfo  mate1ply_drop_tbl[0x10000][COLOR_NB];

  // 玉から見て、dの方向(Directionsの値をpop_lsb()したもの)にpcを置いたときに
  // 大駒の利きを遮断してしまう利きの方向。(遮断しても元の駒の利きがそこに到達しているならそれは遮断しているとはみなさない)
  Directions cutoff_directions[PIECE_NB][8];
}

#ifdef USE_AVX2
using ymm = __m256i;
static const ymm ymm_zero = _mm256_setzero_si256();
static const ymm ymm_one = _mm256_set1_epi8(1);
#endif

// 利きの数や遠方駒の利きを表現するByteBoard
struct ByteBoard
{
  // ある升の利きの数
  uint8_t count(Square sq) const { return e[sq+ array_offset]; }

  // ある升にある長い利きの方向
  Directions directions(Square sq) const { return (Directions)e[sq + array_offset]; }

  // ゼロクリア
  void clear() { memset(e, 0, 128); }

  // ある升の周辺8近傍の利きを取得。1以上の値のところが1になる。壁のところは0になる。
  uint8_t around8(Square sq) const {
#ifdef USE_AVX2
    ymm y0 = _mm256_load_si256((__m256i*)&e[sq + array_offset - 10]);
    ymm mask0 = _mm256_cmpgt_epi8(y0, ymm_zero);
    uint32_t m0 = _mm256_movemask_epi8(mask0); // sqの升の右上の升から32升分は取得できたので、これをPEXTで回収する。
    return PEXT32(m0, 0b111000000101000000111);
#else
    uint8_t a8 = 0;
    if (e[sq + array_offset - 10]) a8 |=   1;
    if (e[sq + array_offset -  9]) a8 |=   2;
    if (e[sq + array_offset -  8]) a8 |=   4;
    if (e[sq + array_offset -  1]) a8 |=   8;
    if (e[sq + array_offset +  1]) a8 |=  16;
    if (e[sq + array_offset +  8]) a8 |=  32;
    if (e[sq + array_offset +  9]) a8 |=  64;
    if (e[sq + array_offset + 10]) a8 |= 128;
    return (uint8_t)a8;
#endif
  }

  // ある升の周辺8近傍の利きを取得。2以上の値のところが1になる。壁のところは0になる。
  uint8_t around8_larger_than_two(Square sq)
  {
#ifdef USE_AVX2_
    ymm y0 = _mm256_load_si256((__m256i*)&e[sq + array_offset - 10]);
    ymm mask0 = _mm256_cmpgt_epi8(y0, ymm_one);
    uint32_t m0 = _mm256_movemask_epi8(mask0); // sqの升の右上の升から32升分は取得できたので、これをPEXTで回収する。
    return PEXT32(m0, 0b111000000101000000111);
#else
    uint8_t a8 = 0;
    if (e[sq + array_offset - 10] > 1 ) a8 |=   1;
    if (e[sq + array_offset - 9]  > 1 ) a8 |=   2;
    if (e[sq + array_offset - 8]  > 1 ) a8 |=   4;
    if (e[sq + array_offset - 1]  > 1 ) a8 |=   8;
    if (e[sq + array_offset + 1]  > 1 ) a8 |=  16;
    if (e[sq + array_offset + 8]  > 1 ) a8 |=  32;
    if (e[sq + array_offset + 9]  > 1 ) a8 |=  64;
    if (e[sq + array_offset + 10] > 1 ) a8 |= 128;
    return (uint8_t)a8;
#endif
  }

  // --- 各升の利きの数

  static const int array_offset = 16;
  static const int array_size = 128;

  // SQ_NB + offset + offset。= SQ_11に対して 8近傍や桂の場所の利きを取ってきたいのでこんなレイアウト。
  uint8_t e[array_size]; // [sq + offset] のようにして使う。

};

// 各升の利きの数
ByteBoard effect[COLOR_NB];

// 長い利き。
ByteBoard long_effect[COLOR_NB];

// 超高速1手詰め判定。
//  info : mate1ply_drop_tbl[]のindex
template <Color Us>
Move Position::mate1ply_impl() const
{
  Square from, to;

  // --- 駒打ちによる詰み
  uint32_t info = 0;

  // 打つことで詰ませられる候補の駒(歩は打ち歩詰めになるので除外されている)
  auto& mi = mate1ply_drop_tbl[info][Us];

  // 持っている手駒の種類
  auto ourHand = toHandKind(hand[Us]);

  // 歩と桂はあとで移動の指し手のところでチェックするのでいまは問題としない。
  auto hk = (HandKind)(ourHand & ~(HAND_KIND_PAWN | HAND_KIND_KNIGHT)) & mi.hand_kind;

  // 駒打ちで詰む条件を満たしていない。
  if (!hk) goto MOVE_MATE;

  auto them = ~Us;
  auto themKing = kingSquare[them];

  // 解説)

  // directions : PIECE 打ちで詰む可能性のある、玉から見た方角
  //  ※　大駒の利きを遮断していなければこれで詰む。
  // to         : PIECEを実際に打つ升
  // cut_off    : toの升で大駒の利きを遮断しているとしたら、その方角
  //  ※  それを補償する利きがあるかを調べる)
  // to2        : 大駒の利きが遮断されたであろう、詰みに関係する升
  //  ※　to2の升は、mate1ply_drop_tblがうまく作られていて、(2)の条件により盤外は除外されている。
  // toにPIECEを打つことでto2の升の大駒の利きが1つ減るが、いまのto2の地点の利きが1より大きければ、
  // 他の駒が利いているということでto2の升に関しては問題ない。

#define CHECK_PIECE(DROP_PIECE) \
  if (hk & HAND_KIND_ ## DROP_PIECE) {                                                              \
    uint32_t directions = mi.directions[DROP_PIECE];                                                \
    Piece pc = make_piece(DROP_PIECE, Us);                                                          \
    while (directions) {                                                                            \
      Direct to_direct = pop_directions(directions);                                                \
      to = themKing + DirectToDelta(to_direct);                                                     \
      uint32_t cut_off = cutoff_directions[pc][to_direct] & long_effect[Us].directions(to);         \
      while (cut_off) {                                                                             \
        Direct cut_direction = pop_directions(cut_off);                                             \
        Square to2 = to + DirectToDelta(cut_direction);                                             \
          if (effect[Us].count(to2) <= 1)                                                           \
          goto Next ## DROP_PIECE;                                                                  \
      }                                                                                             \
      return make_move_drop(GOLD, to);                                                              \
    Next ## DROP_PIECE:;                                                                            \
    }                                                                                               \
  }

  // 一番詰みそうな金から調べていく
  CHECK_PIECE(GOLD);
  CHECK_PIECE(SILVER);
  CHECK_PIECE(ROOK) else CHECK_PIECE(LANCE); // 飛車打ちで詰まないときに香打ちで詰むことはないのでチェックを除外
  CHECK_PIECE(BISHOP);

#undef CHECK_PIECE

MOVE_MATE:

  // --- 移動による詰み

  auto& pinned = state()->checkInfo.pinned;

  // 玉の逃げ道がないことはわかっているのであとは桂が打ててかつ、その場所に敵の利きがなければ詰む。
  // あるいは、桂馬を持っていないとして、その地点に桂馬を跳ねれば詰む。
  if (mi.hand_kind & HAND_KIND_KNIGHT)
  {
    auto drop_target = knightEffect(them, themKing) & ~pieces();
    while (drop_target) {
      to = drop_target.pop();
      if (!effect[them].count(to))
      {
        // 桂馬を持っているならここに打って詰み
        if (ourHand & HAND_KIND_KNIGHT) return make_move_drop(KNIGHT, to);

        // toに利く桂があるならそれを跳ねて詰み。ただしpinされていると駄目
        auto froms = knightEffect(them, to);
        while (froms) {
          from = froms.pop();
          if (!(pinned & from))
            return make_move(from, to);
        }
      }
    }
  }

  // ここで判定するのは近接王手におる詰みのみ。両王手による詰みは稀なので除外。
  // 対象 a) 敵玉の8近傍で、味方の利きが２つ以上ある升
  //   a)に駒を移動させることによる詰みを調べる。
  // 移動させた升において遠方駒の利きを遮断するかどうかのチェックが必要
  //   a)があること自体が稀なのでこのチェックを高速化しして、a)があったときの処理はそれほど高速化に力を入れない。
  // (レアケースでしか起きないことのために大きなテーブルを使うわけにはいかない)
  // 影の利きがあって詰むパターンは対象外。



  return MOVE_NONE;
}

Move Position::mate1ply() const
{
  return sideToMove == BLACK ? mate1ply_impl<BLACK>() : mate1ply_impl<WHITE>();
}

#endif
