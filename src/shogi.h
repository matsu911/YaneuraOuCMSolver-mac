#ifndef _SHOGI_H_
#define _SHOGI_H_

//
//  やねうら王mini
//  公式サイト :  http://yaneuraou.yaneu.com/yaneuraou_mini/
//

// 思考エンジンのバージョンとしてUSIプロトコルの"usi"コマンドに応答するときの文字列
#define Version "0.81"

// --------------------
// コンパイル時の設定
// --------------------

// --- デバッグ時の標準出力への局面表示などに日本語文字列を用いる。

#define PRETTY_JP

// --- ターゲットCPUの選択

// AVX2(Haswell以降)でサポートされた命令を使うか。
// このシンボルをdefineしなければ、pext命令をソフトウェアでエミュレートする。
// 古いCPUのPCで開発をしたていて、遅くてもいいからともかく動いて欲しいときにそうすると良い。

/* #define USE_AVX2 */

// SSE4.2以降でサポートされた命令を使うか。
// このシンボルをdefineしなければ、popcnt命令をソフトウェアでエミュレートする。
// 古いCPUのPCで開発をしたていて、遅くてもいいからともかく動いて欲しいときにそうすると良い。

#define USE_SSE42

// --- assertのレベルを6段階で。
//  ASSERT_LV 0 : assertなし(全体的な処理が速い)
//  ASSERT_LV 1 : 軽量なassert
//  　　　…
//  ASSERT_LV 5 : 重度のassert(全体的な処理が遅い)
// あまり重度のassertにすると、探索性能が落ちるので時間当たりに調べられる局面数が低下するから
// そのへんのバランスをユーザーが決めれるようにこの仕組みを導入。

#define ASSERT_LV 3

// --- USI拡張コマンドの"test"コマンドを有効にする。
// 非常にたくさんのテストコードが書かれているのでコードサイズが膨らむため、
// 思考エンジンとしてリリースするときはコメントアウトしたほうがいいと思う。

#define ENABLE_TEST_CMD

// --- StateInfoに直前の指し手、移動させた駒などの情報を保存しておくのか
// これが保存されていると詰将棋ルーチンなどを自作する場合においてそこまでの手順を表示するのが簡単になる。
// (Position::moves_from_start_pretty()などにより、わかりやすい手順が得られる。
// ただし通常探索においてはやや遅くなるので思考エンジンとしてリリースするときには無効にしておくこと。

#define KEEP_LAST_MOVE

// 協力詰め用思考エンジンなどで評価関数を使わないときにまで評価関数用のテーブルを
// 確保するのはもったいないので、そのテーブルを確保するかどうかを選択するためのオプション。

//#define USE_EVAL_TABLE

// 超高速1手詰め判定ルーチンを用いるか。(やねうら王nanoでは削除予定)

#define MATE_1PLY

// 通例hash keyは64bitだが、これを128にするとPosition::state()->long_key()から128bit hash keyが
// 得られるようになる。研究時に局面が厳密に合致しているかどうかを判定したいときなどに用いる。
// ※　やねうら王nanoではこの機能は削除する予定。
#define HASH_KEY_BITS 64
//#define HASH_KEY_BITS 128
//#define HASH_KEY_BITS 256

// 通常探索時の最大探索深さ
#define MAX_PLY_ 128

// 協力詰めsolverとしてリリースする場合。
// 協力詰めの最長は49909。「寿限無3」 cf. http://www.ne.jp/asahi/tetsu/toybox/kato/fbaka4.htm
#define COOPERATIVE_MATE_SOLVER

// --------------------
// release configurations
// --------------------

// --- 思考エンジンとしてバイナリを公開するとき用の設定集

#if 0 // 思考エンジンとしてリリースする場合(実行速度を求める場合)
#undef ASSERT_LV
#undef KEEP_LAST_MOVE
#undef ENABLE_TEST_CMD
#endif

#ifdef COOPERATIVE_MATE_SOLVER
#undef ASSERT_LV
#define KEEP_LAST_MOVE
#undef  MAX_PLY_
#define MAX_PLY_ 65000
#undef HASH_KEY_BITS
#define HASH_KEY_BITS 128
#undef USE_EVAL_TABLE
//#undef MATE_1PLY
#endif

// --------------------
// include & configure
// --------------------

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <stack>
#include <memory>
#include <map>
#include <iostream>

// --- うざいので無効化するwarning
// C4800 : 'unsigned int': ブール値を 'true' または 'false' に強制的に設定します
// →　static_cast<bool>(...)において出る。
#pragma warning(disable : 4800)

// --- assertion tools

// DEBUGビルドでないとassertが無効化されてしまうので無効化されないASSERT
// 故意にメモリアクセス違反を起こすコード。
#define ASSERT(X) { if (!(X)) *(int*)0 =0; }

// ASSERT LVに応じたassert
#ifndef ASSERT_LV
#define ASSERT_LV 0
#endif

#define ASSERT_LV_EX(L, X) { if (L <= ASSERT_LV) ASSERT(X); }
#define ASSERT_LV1(X) ASSERT_LV_EX(1, X)
#define ASSERT_LV2(X) ASSERT_LV_EX(2, X)
#define ASSERT_LV3(X) ASSERT_LV_EX(3, X)
#define ASSERT_LV4(X) ASSERT_LV_EX(4, X)
#define ASSERT_LV5(X) ASSERT_LV_EX(5, X)

// --- switchにおいてdefaultに到達しないことを明示して高速化させる
#ifdef _DEBUG
// デバッグ時は普通にしとかないと変なアドレスにジャンプして原因究明に時間がかかる。
#define UNREACHABLE ASSERT_LV1(false);
#elif defined(_MSC_VER)
#define UNREACHABLE ASSERT_LV1(false); __assume(0);
#elif defined(__GNUC__)
#define UNREACHABLE __builtin_unreachable();
#else
#define UNREACHABLE ASSERT_LV1(false);
#endif

// PRETTY_JPが定義されているかどうかによって三項演算子などを使いたいので。
#ifdef PRETTY_JP
const bool pretty_jp = true;
#else
const bool pretty_jp = false;
#endif

#if HASH_KEY_BITS <= 64
#define HASH_KEY Key64
#elif HASH_KEY_BITS <= 128
#define HASH_KEY Key128
#else
#define HASH_KEY Key256
#endif

// ターゲットが64bitOSかどうか
#if defined(_WIN64) && defined(_MSC_VER)
const bool Is64Bit = true;
#else
const bool Is64Bit = false;
#endif

// --------------------
//    bit operations
// --------------------

#ifdef USE_AVX2
const bool use_avx2 = true;

// for SSE,AVX,AVX2
#include <immintrin.h>

// for AVX2 : hardwareによるpext実装
#define PEXT32(a,b) _pext_u32(a,b)
#define PEXT64(a,b) _pext_u64(a,b)

#else
const bool use_avx2 = false;

// for non-AVX2 : software emulationによるpext実装(やや遅い。とりあえず動くというだけ。)
inline uint64_t pext(uint64_t val, uint64_t mask)
{
  uint64_t res = 0;
  for (uint64_t bb = 1; mask; bb += bb) {
    if ((int64_t)val & (int64_t)mask & -(int64_t)mask)
      res |= bb;
    // マスクを1bitずつ剥がしていく実装なので処理時間がbit長に依存しない。
    // ゆえに、32bit用のpextを別途用意する必要がない。
    mask &= mask - 1;
  }
  return res;
}

inline uint32_t PEXT32(uint32_t a, uint32_t b) { return (uint32_t)pext(a, b); }
inline uint64_t PEXT64(uint64_t a, uint64_t b) { return pext(a, b); }

#endif

#ifdef USE_SSE42
const bool use_sse42 = true;

// for SSE4.2
#include <intrin.h>
#define POPCNT8(a) __popcnt8(a)
#define POPCNT32(a) __popcnt32(a)
#define POPCNT64(a) __popcnt64(a)

#else
const bool use_sse42 = false;

// software emulationによるpopcnt(やや遅い)
inline int32_t POPCNT8(uint32_t a) {
  a = (a & UINT32_C(0x55)) + (a >> 1 & UINT32_C(0x55));
  a = (a & UINT32_C(0x33)) + (a >> 2 & UINT32_C(0x33));
  a = (a & UINT32_C(0x0f)) + (a >> 4 & UINT32_C(0x0f));
  return (int32_t)a;
}
inline int32_t POPCNT32(uint32_t a) {
  a = (a & UINT32_C(0x55555555)) + (a >> 1 & UINT32_C(0x55555555));
  a = (a & UINT32_C(0x33333333)) + (a >> 2 & UINT32_C(0x33333333));
  a = (a & UINT32_C(0x0f0f0f0f)) + (a >> 4 & UINT32_C(0x0f0f0f0f));
  a = (a & UINT32_C(0x00ff00ff)) + (a >> 8 & UINT32_C(0x00ff00ff));
  a = (a & UINT32_C(0x0000ffff)) + (a >> 16 & UINT32_C(0x0000ffff));
  return (int32_t)a;
}
inline int32_t POPCNT64(uint64_t a) {
  a = (a & UINT64_C(0x5555555555555555)) + (a >> 1 & UINT64_C(0x5555555555555555));
  a = (a & UINT64_C(0x3333333333333333)) + (a >> 2 & UINT64_C(0x3333333333333333));
  a = (a & UINT64_C(0x0f0f0f0f0f0f0f0f)) + (a >> 4 & UINT64_C(0x0f0f0f0f0f0f0f0f));
  a = (a & UINT64_C(0x00ff00ff00ff00ff)) + (a >> 8 & UINT64_C(0x00ff00ff00ff00ff));
  a = (a & UINT64_C(0x0000ffff0000ffff)) + (a >> 16 & UINT64_C(0x0000ffff0000ffff));
  return (int32_t)a + (int32_t)(a >> 32);
}
#endif

// --------------------
//      手番
// --------------------

// 手番
enum Color { BLACK=0/*先手*/,WHITE=1/*後手*/,COLOR_NB /* =2 */ , COLOR_ALL = 2 /*先後共通の何か*/ , COLOR_ZERO = 0,};

// 相手番を返す
constexpr Color operator ~(Color c) { return (Color)(c ^ 1);  }

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(Color c) { return COLOR_ZERO <= c && c < COLOR_NB; }

// 出力用(USI形式ではない)　デバッグ用。
std::ostream& operator<<(std::ostream& os, Color c);

// --------------------
//        筋
// --------------------

//  例) FILE_3なら3筋。
enum File { FILE_1, FILE_2, FILE_3, FILE_4, FILE_5, FILE_6, FILE_7, FILE_8, FILE_9 , FILE_NB , FILE_ZERO=0 };

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(File f) { return FILE_ZERO <= f && f < FILE_NB; }

// USIの指し手文字列などで筋を表す文字列をここで定義されたFileに変換する。
inline File toFile(char c) { return (File)(c - '1'); }

// Fileを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → 八
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 8
std::string pretty(File f);

// USI形式でFileを出力する
inline std::ostream& operator<<(std::ostream& os, File f) { os << (char)('1' + f); return os; }

// --------------------
//        段
// --------------------

// 例) RANK_4なら4段目。
enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9 , RANK_NB , RANK_ZERO = 0};

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(Rank r) { return RANK_ZERO <= r && r < RANK_NB; }

// 移動元、もしくは移動先の升のrankを与えたときに、そこが成れるかどうかを判定する。
inline bool canPromote(const Color c, const Rank fromOrToRank) {
  ASSERT_LV1(is_ok(c) && is_ok(fromOrToRank));
  // 先手9bit(9段) + 後手9bit(9段) = 18bitのbit列に対して、判定すればいい。
  // ただし ×9みたいな掛け算をするのは嫌なのでbit shiftで済むように先手16bit、後手16bitの32bitのbit列に対して判定する。
  // このcastにおいて、VC++2015ではwarning C4800が出る。
  return static_cast<bool>(0x1c00007u & (1u << ((c << 4) + fromOrToRank)));
}

// USIの指し手文字列などで段を表す文字列をここで定義されたRankに変換する。
inline Rank toRank(char c) { return (Rank)(c - 'a'); }

// Rankを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → ８
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 8
std::string pretty(Rank r);

// USI形式でRankを出力する
inline std::ostream& operator<<(std::ostream& os, Rank r) { os << (char)('a' + r); return os; }

// --------------------
//        升目
// --------------------

// 盤上の升目に対応する定数。
// 盤上右上(１一が0)、左下(９九)が80
enum Square : int32_t
{
  // 以下、盤面の右上から左下までの定数。
  // これを定義していなくとも問題ないのだが、デバッガでSquare型を見たときに
  // どの升であるかが表示されることに価値がある。
  SQ_11, SQ_12, SQ_13, SQ_14, SQ_15, SQ_16, SQ_17, SQ_18, SQ_19,
  SQ_21, SQ_22, SQ_23, SQ_24, SQ_25, SQ_26, SQ_27, SQ_28, SQ_29,
  SQ_31, SQ_32, SQ_33, SQ_34, SQ_35, SQ_36, SQ_37, SQ_38, SQ_39,
  SQ_41, SQ_42, SQ_43, SQ_44, SQ_45, SQ_46, SQ_47, SQ_48, SQ_49,
  SQ_51, SQ_52, SQ_53, SQ_54, SQ_55, SQ_56, SQ_57, SQ_58, SQ_59,
  SQ_61, SQ_62, SQ_63, SQ_64, SQ_65, SQ_66, SQ_67, SQ_68, SQ_69,
  SQ_71, SQ_72, SQ_73, SQ_74, SQ_75, SQ_76, SQ_77, SQ_78, SQ_79,
  SQ_81, SQ_82, SQ_83, SQ_84, SQ_85, SQ_86, SQ_87, SQ_88, SQ_89,
  SQ_91, SQ_92, SQ_93, SQ_94, SQ_95, SQ_96, SQ_97, SQ_98, SQ_99,

  // ゼロと末尾
  SQ_ZERO = 0, SQ_NB = 81,
  SQ_NB_PLUS1 = SQ_NB + 1, // 玉がいない場合、SQ_NBに移動したものとして扱うため、配列をSQ_NB+1で確保しないといけないときがあるのでこの定数を用いる。

  // 方角に関する定数。N=北=盤面の下を意味する。
  DELTA_N = +1, // 下
  DELTA_E = -9, // 右
  DELTA_S = -1, // 上
  DELTA_W = +9, // 左

  // 斜めの方角などを意味する定数。
  DELTA_NN = int(DELTA_N) + int(DELTA_N),
  DELTA_NE = int(DELTA_N) + int(DELTA_E),
  DELTA_SE = int(DELTA_S) + int(DELTA_E),
  DELTA_SS = int(DELTA_S) + int(DELTA_S),
  DELTA_SW = int(DELTA_S) + int(DELTA_W),
  DELTA_NW = int(DELTA_N) + int(DELTA_W)
};

// sqが盤面の内側を指しているかを判定する。assert()などで使う用。
// 駒は駒落ちのときにSQ_NBに移動するので、値としてSQ_NBは許容する。
constexpr bool is_ok(Square sq) { return SQ_ZERO <= sq && sq <= SQ_NB; }

// sqが盤面の内側を指しているかを判定する。assert()などで使う用。玉は盤上にないときにSQ_NBを取るのでこの関数が必要。
constexpr bool is_ok_plus1(Square sq) { return SQ_ZERO <= sq && sq < SQ_NB_PLUS1; }

const File SquareToFile[SQ_NB] = {
  FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1,
  FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2,
  FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3,
  FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4,
  FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5,
  FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6,
  FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7,
  FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8,
  FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9
};

// 与えられたSquareに対応する筋を返す。
// →　行数は長くなるが速度面においてテーブルを用いる。
inline File file_of(Square sq) { /* return (File)(sq / 9); */ ASSERT_LV2(is_ok(sq)); return SquareToFile[sq]; }

const Rank SquareToRank[SQ_NB] = {
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
};

// 与えられたSquareに対応する段を返す。
// →　行数は長くなるが速度面においてテーブルを用いる。
inline Rank rank_of(Square sq) { /* return (Rank)(sq % 9); */ ASSERT_LV2(is_ok(sq)); return SquareToRank[sq]; }

// 筋(File)と段(Rank)から、それに対応する升(Square)を返す。
inline Square operator | (File f, Rank r) { Square sq = (Square)(f * 9 + r); ASSERT_LV2(is_ok(sq)); return sq; }

// ２つの升のマンハッタン距離を返す
inline int dist(Square sq1, Square sq2) { return abs(file_of(sq1)-file_of(sq2)) + abs(rank_of(sq1) - rank_of(sq2)); }

// 移動元、もしくは移動先の升sqを与えたときに、そこが成れるかどうかを判定する。
inline bool canPromote(const Color c, const Square fromOrTo) {
  ASSERT_LV2(is_ok(fromOrTo));
  return canPromote(c, rank_of(fromOrTo));
}

// 盤面を180°回したときの升目を返す
inline Square Inv(Square sq) { return (Square)((SQ_NB - 1) - sq); }

// Squareを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → ８八
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 88
inline std::string pretty(Square sq) { return pretty(file_of(sq)) + pretty(rank_of(sq)); }

// USI形式でSquareを出力する
inline std::ostream& operator<<(std::ostream& os, Square sq) { os << file_of(sq) << rank_of(sq); return os; }

// --------------------
//        方角
// --------------------

// 2つの升の関係が縦横斜めのどれであるかを表すときに使う方角の定数。
enum Direction : int8_t
{
  DIRECTION_MISC  = 0, // 縦横斜めの関係にない
  DIRECTION_DIAG1 = 1, // 斜め その1(左下から右上方向)
  DIRECTION_DIAG2 = 2, // 斜め その2(左上から右下方向)
  DIRECTION_RANK  = 3, // 段(横)
  DIRECTION_FILE  = 4, // 筋(縦)
};

// 2つの升がどの方角であるかを返すテーブル。
extern Direction Direc[SQ_NB_PLUS1][SQ_NB_PLUS1];

// 与えられた3升が縦横斜めの1直線上にあるか。駒を移動させたときに開き王手になるかどうかを判定するのに使う。
// 例) 王がsq1, pinされている駒がsq2にあるときに、pinされている駒をsq3に移動させたときにis_aligned(sq1,sq2,sq3)であれば、
//  pinされている方向に沿った移動なので開き王手にはならないと判定できる。
inline bool is_aligned(Square sq1, Square sq2, Square sq3)
{
  auto d1 = Direc[sq1][sq2];
  if (d1 == DIRECTION_MISC)
    return false;
  return d1 == Direc[sq1][sq3];
}

// --------------------
//     探索深さ
// --------------------

// Depthは1手をONE_PLY倍にスケーリングする。
enum Depth : int16_t{ ONE_PLY = 2 };

// 通常探索時の最大探索深さ
const int MAX_PLY = MAX_PLY_;

// 静止探索時の最大探索深さ
const int MAX_QUIET_PLY = 6;

// 通常探索時の最大手数である128に、静止探索時の最大深さを加えた定数。
// 局面バッファ(StateInfo)などは、すべてこのサイズで確保する。
const int MAX_SEARCH_PLY = MAX_PLY + MAX_QUIET_PLY;

// --------------------
//     評価値の性質
// --------------------

// searchで探索窓を設定するので、この窓の範囲外の値が返ってきた場合、
// high fail時はこの値は上界(真の値はこれより小さい)、low fail時はこの値は下界(真の値はこれより大きい)
// である。
enum Bound {
  BOUND_NONE,  // 探索していない(DEPTH_NONE)ときに、最善手か、静的評価スコアだけを置換表に格納したいときに用いる。
  BOUND_UPPER, // 上界(真の評価値はこれより小さい)
  BOUND_LOWER, // 下界(真の評価値はこれより大きい)
  BOUND_EXACT = BOUND_UPPER | BOUND_LOWER // 真の評価値と一致している。
};

// --------------------
//        評価値
// --------------------

// 置換表に格納するときにあまりbit数が多いともったいないので16bitで。
enum Value : int16_t
{
  VALUE_ZERO = 0,

  // Valueの取りうる最大値(最小値はこの符号を反転させた値)
  VALUE_INFINITE = INT16_MAX - 1, // 最小値として-VALUE_INFINITEを使うときにoverflowしないようにINT16_MAXから1引いておく。

  VALUE_MATE = VALUE_INFINITE - 1, // 1手詰めのスコア(例えば、3手詰めならこの値より2少ない)

  VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY,   // MAX_PLYでの詰みのときのスコア。
  VALUE_MATEED_IN_MAX_PLY = -(int)VALUE_MATE + MAX_PLY, // MAX_PLYで詰まされるときのスコア。
};

// --------------------
//        駒
// --------------------

enum Piece : int32_t
{
  // 金の順番を飛の後ろにしておく。KINGを8にしておく。
  // こうすることで、成りを求めるときに pc |= 8;で求まり、かつ、先手の全種類の駒を列挙するときに空きが発生しない。(DRAGONが終端になる)
  NO_PIECE, PAWN/*歩*/, LANCE/*香*/, KNIGHT/*桂*/, SILVER/*銀*/, BISHOP/*角*/, ROOK/*飛*/, GOLD/*金*/ ,
  KING = 8/*玉*/, PRO_PAWN /*と*/, PRO_LANCE /*成香*/, PRO_KNIGHT /*成桂*/, PRO_SILVER /*成銀*/, HORSE/*馬*/, DRAGON/*龍*/, PRO_GOLD/*未使用*/,
  // 以下、先後の区別のある駒(Bがついているのは先手、Wがついているのは後手)
  B_PAWN = 1 , B_LANCE, B_KNIGHT, B_SILVER, B_BISHOP, B_ROOK, B_GOLD , B_KING, B_PRO_PAWN, B_PRO_LANCE, B_PRO_KNIGHT, B_PRO_SILVER, B_HORSE, B_DRAGON, B_T_GOLD,
  W_PAWN = 17, W_LANCE, W_KNIGHT, W_SILVER, W_BISHOP, W_ROOK, W_GOLD , W_KING, W_PRO_PAWN, W_PRO_LANCE, W_PRO_KNIGHT, W_PRO_SILVER, W_HORSE, W_DRAGON, W_T_GOLD,
  PIECE_NB, // 終端
  PIECE_ZERO = 0,

  // --- 特殊な定数

  PIECE_PROMOTE = 8, // 成り駒と非成り駒との差(この定数を足すと成り駒になる)
  PIECE_WHITE = 16,  // これを先手の駒に加算すると後手の駒になる。
  PIECE_RAW_NB = 8,  // 非成駒の終端

  PIECE_HAND_ZERO = PAWN, // 手駒の開始位置
  PIECE_HAND_NB = KING  , // 手駒になる駒種の最大+1

  HDK = KING,       // Position::pieces()で使うときの定数。H=Horse,D=Dragon,K=Kingの合体したBitboardが返る。

  // 指し手生成(GeneratePieceMove = GPM)でtemplateの引数として使うマーカー的な値。変更する可能性があるのでユーザーは使わないでください。
  GPM_BR   = 100 ,     // Bishop Rook
  GPM_GBR  = 101 ,     // Gold Bishop Rook
  GPM_GHD  = 102 ,     // Gold Horse Dragon
  GPM_GHDK = 103 ,     // Gold Horse Dragon King
};

// USIプロトコルで駒を表す文字列を返す。
inline std::string usi_piece(Piece pc) { return std::string(". P L N S B R G K +P+L+N+S+B+R+G+.p l n s b r g k +p+l+n+s+b+r+g+k").substr(pc * 2, 2); }

// 駒に対して、それが先後、どちらの手番の駒であるかを返す。
constexpr Color color_of(Piece pc) { return (pc & PIECE_WHITE) ? WHITE : BLACK; }

// 後手の歩→先手の歩のように、後手という属性を取り払った駒種を返す
constexpr Piece type_of(Piece pc) { return (Piece)(pc & 15); }

// 成ってない駒を返す。後手という属性も消去する。
// 例) 成銀→銀 , 後手の馬→先手の角
// ただし、pc == KINGでの呼び出しはNO_PIECEが返るものとする。
constexpr Piece raw_type_of(Piece pc) { return (Piece)(pc & 7); }

// pcとして先手の駒を渡し、cが後手なら後手の駒を返す。cが先手なら先手の駒のまま。pcとしてNO_PIECEは渡してはならない。
inline Piece make_piece(Piece pt, Color c) { ASSERT_LV3(color_of(pt) == BLACK && pt!=NO_PIECE);  return (Piece)(pt + (c << 4)); }

// Pieceの整合性の検査。assert用。
constexpr bool is_ok(Piece pc) { return NO_PIECE <= pc && pc < PIECE_NB; }

// Pieceを綺麗に出力する(USI形式ではない) 先手の駒は大文字、後手の駒は小文字、成り駒は先頭に+がつく。盤面表示に使う。
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。
std::string pretty(Piece pc);

// ↑のpretty()だと先手の駒を表示したときに先頭にスペースが入るので、それが嫌な場合はこちらを用いる。
inline std::string pretty2(Piece pc) { ASSERT_LV1(color_of(pc) == BLACK); auto s = pretty(pc); return s.substr(1, s.length() - 1); }

// USIで、盤上の駒を表現する文字列
// ※　歩Pawn 香Lance 桂kNight 銀Silver 角Bishop 飛Rook 金Gold 王King
const std::string PieceToCharBW(" PLNSBRGK        plnsbrgk");

// PieceをUSI形式で表示する。
std::ostream& operator<<(std::ostream& os, Piece pc);

// --------------------
//        駒箱
// --------------------

// Positionクラスで用いる、駒リスト(どの駒がどこにあるのか)を管理するときの番号。
enum PieceNo {
  PIECE_NO_PAWN = 0, PIECE_NO_LANCE = 18, PIECE_NO_KNIGHT = 22, PIECE_NO_SILVER = 26,
  PIECE_NO_GOLD = 30, PIECE_NO_BISHOP = 34, PIECE_NO_ROOK = 36, PIECE_NO_KING = 38,
  PIECE_NO_BKING = 38, PIECE_NO_WKING = 39, // 先手、後手の玉の番号が必要な場合はこっちを用いる
  PIECE_NO_ZERO = 0, PIECE_NO_NB = 40,
};

// PieceNoの整合性の検査。assert用。
constexpr bool is_ok(PieceNo pn) { return PIECE_NO_ZERO <= pn && pn < PIECE_NO_NB; }

// --------------------
//       指し手
// --------------------

// 指し手 bit0..6 = 移動先のSquare、bit7..13 = 移動元のSquare(駒打ちのときは駒種)、bit14..駒打ちか、bit15..成りか
enum Move : uint16_t {

  MOVE_NONE = 0, // 無効な移動
  MOVE_NULL = (1 << 7) + 1, // NULL MOVEを意味する指し手。Square(1)からSquare(1)への移動は存在しないのでここを特殊な記号として使う。
  MOVE_RESIGN = (2 << 7) + 2,// << で出力したときに"resign"と表示する投了を意味する指し手。
  MOVE_DROP = 1 << 14, // 駒打ちフラグ
  MOVE_PROMOTE = 1 << 15, // 駒成りフラグ
};

// 指し手の移動元の升を返す
constexpr Square move_from(Move m) { return Square((m >> 7) & 0x7f); }

// 指し手の移動先の升を返す
constexpr Square move_to(Move m) { return Square(m & 0x7f); }

// 指し手が駒打ちか？
constexpr bool is_drop(Move m){ return (m & MOVE_DROP)!=0; }

// 指し手が成りか？
constexpr bool is_promote(Move m) { return (m & MOVE_PROMOTE)!=0; }

// 駒打ち(is_drop()==true)のときの打った駒
constexpr Piece move_dropped_piece(Move m) { return (Piece)move_from(m); }

// fromからtoに移動する指し手を生成して返す
constexpr Move make_move(Square from, Square to) { return (Move)(to + (from << 7)); }

// fromからtoに移動する、成りの指し手を生成して返す
constexpr Move make_move_promote(Square from, Square to) { return (Move)(to + (from << 7) + MOVE_PROMOTE); }

// Pieceをtoに打つ指し手を生成して返す
constexpr Move make_move_drop(Piece pt, Square to) { return (Move)(to + (pt << 7) + MOVE_DROP); }

// 指し手がおかしくないかをテストする
// ただし、盤面のことは考慮していない。MOVE_NULLとMOVE_NONEであるとfalseが返る。
// これら２つの定数は、移動元と移動先が等しい値になっている。このテストだけをする。
inline bool is_ok(Move m) {
  // return move_from(m)!=move_to(m);
  // とやりたいところだが、駒打ちでfromのbitを使ってしまっているのでそれだとまずい。
  // 駒打ちのbitも考慮に入れるために次のように書く。
  return (m >> 7) != (m & 0x7f);
}

// 見た目に、わかりやすい形式で表示する
std::string pretty(Move m);

// 移動させた駒がわかっているときに指し手をわかりやすい表示形式で表示する。
std::string pretty(Move m, Piece movedPieceType);

// USI形式で指し手を表示する
std::ostream& operator<<(std::ostream& os, Move m);

// --------------------
//   拡張された指し手
// --------------------

// 指し手とオーダリングのためのスコアがペアになっている構造体。
// オーダリングのときにスコアで並べ替えしたいが、一つになっているほうが並び替えがしやすいのでこうしてある。
struct ExtMove {

  Move move;   // 指し手
  Value value; // これはMovePickerが指し手オーダリングのために並び替えるときに用いる値(≠評価値)。

  // Move型とは暗黙で変換できていい。

  operator Move() const { return move; }
  void operator=(Move m) { move = m; }
};

// ExtMoveの並べ替えを行なうので比較オペレーターを定義しておく。
inline bool operator<(const ExtMove& first, const ExtMove& second) {
  return first.value < second.value;
}

inline std::ostream& operator<<(std::ostream& os, ExtMove m) { os << m.move << '(' << m.value << ')'; return os; }

// --------------------
//       手駒
// --------------------

// 手駒
// 歩の枚数を8bit、香、桂、銀、角、飛、金を4bitずつで持つ。こうすると16進数表示したときに綺麗に表示される。(なのはのアイデア)
enum Hand : int32_t { HAND_ZERO = 0, };

// 手駒のbit位置
constexpr int PIECE_BITS[PIECE_HAND_NB] = { 0, 0 /*歩*/, 8 /*香*/, 12 /*桂*/, 16 /*銀*/, 20 /*角*/, 24 /*飛*/ , 28 /*金*/ };

// Piece(歩,香,桂,銀,金,角,飛)を手駒に変換するテーブル
constexpr Hand PIECE_TO_HAND[PIECE_HAND_NB] = { (Hand)0, (Hand)(1 << PIECE_BITS[PAWN]) /*歩*/, (Hand)(1 << PIECE_BITS[LANCE]) /*香*/, (Hand)(1 << PIECE_BITS[KNIGHT]) /*桂*/,
(Hand)(1 << PIECE_BITS[SILVER]) /*銀*/,(Hand)(1 << PIECE_BITS[BISHOP]) /*角*/,(Hand)(1 << PIECE_BITS[ROOK]) /*飛*/,(Hand)(1 << PIECE_BITS[GOLD]) /*金*/ };

// その持ち駒を表現するのに必要なbit数のmask(例えば3bitなら2の3乗-1で7)
constexpr int PIECE_BIT_MASK[PIECE_HAND_NB] = { 0,31/*歩は5bit*/,7/*香は3bit*/,7/*桂*/,7/*銀*/,3/*角*/,3/*飛*/,7/*金*/ };

constexpr int PIECE_BIT_MASK2[PIECE_HAND_NB] = { 0,
  PIECE_BIT_MASK[PAWN]   << PIECE_BITS[PAWN]  , PIECE_BIT_MASK[LANCE]  << PIECE_BITS[LANCE] , PIECE_BIT_MASK[KNIGHT] << PIECE_BITS[KNIGHT],
  PIECE_BIT_MASK[SILVER] << PIECE_BITS[SILVER], PIECE_BIT_MASK[BISHOP] << PIECE_BITS[BISHOP], PIECE_BIT_MASK[ROOK]   << PIECE_BITS[ROOK]  ,
  PIECE_BIT_MASK[GOLD]   << PIECE_BITS[GOLD] };

// 駒の枚数が格納されているbitが1となっているMASK。(駒種を得るときに使う)
constexpr int32_t HAND_BIT_MASK = PIECE_BIT_MASK2[PAWN] | PIECE_BIT_MASK2[LANCE] | PIECE_BIT_MASK2[KNIGHT] | PIECE_BIT_MASK2[SILVER]
          | PIECE_BIT_MASK2[BISHOP] | PIECE_BIT_MASK2[ROOK] | PIECE_BIT_MASK2[GOLD];

// 余らせてあるbitの集合。
constexpr int32_t HAND_BORROW_MASK = (HAND_BIT_MASK << 1) & ~HAND_BIT_MASK;

// 手駒pcの枚数を返す。
inline int hand_count(Hand hand, Piece pr) { ASSERT_LV2(PIECE_HAND_ZERO <= pr && pr < PIECE_HAND_NB); return (hand >> PIECE_BITS[pr]) & PIECE_BIT_MASK[pr]; }

// 手駒pcを持っているかどうかを返す。
inline int hand_exists(Hand hand, Piece pr) { ASSERT_LV2(PIECE_HAND_ZERO <= pr && pr < PIECE_HAND_NB); return hand & PIECE_BIT_MASK2[pr]; }

// 手駒にpcをc枚加える
inline void add_hand(Hand &hand, Piece pr, int c=1) { hand = (Hand)(hand + PIECE_TO_HAND[pr] * c); }

// 手駒からpcをc枚減ずる
inline void sub_hand(Hand &hand, Piece pr, int c=1) { hand = (Hand)(hand - PIECE_TO_HAND[pr] * c); }

// 手駒h1のほうがh2より優れているか。(すべての種類の手駒がh2のそれ以上ある)
// 優等局面の判定のとき、局面のhash key(StateInfo::key() )が一致していなくて、盤面のhash key(StateInfo::key_board() )が
// 一致しているときに手駒の比較に用いるので、手駒がequalというケースは前提により除外されているから、この関数を以ってsuperiorであるという判定が出来る。
inline bool hand_is_equal_or_superior(Hand h1, Hand h2) { return ((h1-h2) & HAND_BORROW_MASK) == 0; }

// 手駒を表示する(USI形式ではない) デバッグ用
std::ostream& operator<<(std::ostream& os, Hand hand);

// --------------------
// 手駒情報を直列化したもの
// --------------------

// 特定種の手駒を持っているかどうかをbitで表現するクラス
// bit0..歩を持っているか , bit1..香 , bit2..桂 , bit3..銀 , bit4..角 , bit5..飛 , bit6..金
enum HandKind : uint32_t { HAND_KIND_PAWN = 1 << (PAWN-1), HAND_KIND_LANCE=1 << (LANCE-1) , HAND_KIND_KNIGHT = 1 << (KNIGHT-1),
  HAND_KIND_SILVER = 1 << (SILVER-1), HAND_KIND_BISHOP = 1 << (BISHOP-1), HAND_KIND_ROOK = 1 << (ROOK-1) , HAND_KIND_GOLD = 1 << (GOLD-1) };

// Hand型からHandKind型への変換子
// 例えば歩の枚数であれば5bitで表現できるが、011111bを加算すると1枚でもあれば桁あふれしてbit5が1になる。
// これをPEXT32で回収するという戦略。
inline HandKind toHandKind(Hand h) {return (HandKind)PEXT32(h + HAND_BIT_MASK, HAND_BORROW_MASK);}

// 特定種類の駒を持っているかを判定する
inline bool hand_exists(HandKind hk, Piece pt) { ASSERT_LV2(PIECE_HAND_ZERO <= pt && pt < PIECE_HAND_NB);  return static_cast<bool>(hk & (1 << (pt - 1))); }

// 歩以外の手駒を持っているかを判定する
inline bool hand_exceptPawnExists(HandKind hk) { return hk & ~HAND_KIND_PAWN; }

// 手駒の有無を表示する(USI形式ではない) デバッグ用
std::ostream& operator<<(std::ostream& os, HandKind hk);

// --------------------
//    指し手生成器
// --------------------

// 将棋のある局面の合法手の最大数。593らしいが、保険をかけて少し大きめにしておく。
const int MAX_MOVES = 600;

// 生成する指し手の種類
enum MOVE_GEN_TYPE
{
  // LEGAL/LEGAL_ALL以外は自殺手が含まれることがある(pseudo-legal)ので、do_moveの前にPosition::legal()でのチェックが必要。

  NON_CAPTURES,	// 駒を取らない指し手
  CAPTURES,			// 駒を取る指し手

  CAPTURES_PRO_PLUS,      // CAPTURES + 価値のかなりあると思われる成り(歩だけ)
  NON_CAPTURES_PRO_MINUS, // NON_CAPTURES - 価値のかなりあると思われる成り(歩だけ)

  // BonanzaではCAPTURESに銀以外の成りを含めていたが、Aperyでは歩の成り以外は含めない。
  // あまり変な成りまで入れるとオーダリングを阻害する。
  // 本ソースコードでは、NON_CAPTURESとCAPTURESは使わず、CAPTURES_PRO_PLUSとNON_CAPTURES_PRO_MINUSを使う。

  // note : NON_CAPTURESとCAPTURESとの生成される指し手の集合は被覆していない。
  // note : CAPTURES_PRO_PLUSとNON_CAPTURES_PRO_MINUSとの生成される指し手の集合も被覆していない。
  // →　被覆させないことで、二段階に指し手生成を分解することが出来る。

  EVASIONS ,            // 王手の回避(指し手生成元で王手されている局面であることがわかっているときはこちらを呼び出す)
  NON_EVASIONS,         // 王手の回避ではない手(指し手生成元で王手されていない局面であることがわかっているときのすべての指し手)
  EVASIONS_ALL,         // EVASIONS + 歩の不成なども含む

  // 以下の2つは、pos.legalを内部的に呼び出すので生成するのに時間が少しかかる。棋譜の読み込み時などにしか使わない。
  LEGAL,                // 合法手すべて。ただし、2段目の歩・香の不成や角・飛の不成は生成しない。
  LEGAL_ALL,            // 合法手すべて

  // 以下の2つは、やねうら王nanoでは削除予定
  CHECKS,               // 王手となる指し手(歩の不成などは含まない)
  CHECKS_ALL,           // 王手となる指し手(歩の不成なども含む)
};

struct Position; // 前方宣言

// 指し手を生成器本体
// gen_typeとして生成する指し手の種類をシてする。gen_allをfalseにすると歩の不成、香の8段目の不成は生成しない。通常探索中はそれでいいはず。
// mlist : 指し手を返して欲しい指し手生成バッファのアドレス
// 返し値 : 生成した指し手の終端
struct CheckInfo;
template <MOVE_GEN_TYPE gen_type>
ExtMove* generateMoves(const Position& pos, ExtMove* mlist);

// MoveGeneratorのwrapper。範囲forで回すときに便利。
template<MOVE_GEN_TYPE GenType>
struct MoveList {
  // 局面をコンストラクタの引数に渡して使う。すると指し手が生成され、lastが初期化されるので、
  // このclassのbegin(),end()が正常な値を返すようになる。
  // CHECKS,CHECKS_NON_PRO_PLUSを生成するときは、事前にpos.check_info_update();でCheckInfoをupdateしておくこと。
  explicit MoveList(const Position& pos) : last(generateMoves<GenType>(pos, mlist)){}

  // 内部的に持っている指し手生成バッファの先頭
  const ExtMove* begin() const { return mlist; }

  // 生成された指し手の末尾のひとつ先
  const ExtMove* end() const { return last; }

  // 生成された指し手の数
  size_t size() const { return last - mlist; }

  // i番目の要素を返す
  const ExtMove at(size_t i) const { ASSERT_LV3(0<=i && i<size()); return begin()[i]; }

private:
  // 指し手生成バッファも自前で持っている。
  ExtMove mlist[MAX_MOVES], *last;
};

// --------------------
//    bit operations
// --------------------

// 1である最下位bitを1bit取り出して、そのbit位置を返す。
// 0を渡してはならない。
inline int pop_lsb(uint64_t& b){
  unsigned long index;
  _BitScanForward64(&index, b);
  b &= b - 1;
  return index;
}

inline int pop_lsb(uint32_t& b) {
  unsigned long index;
  _BitScanForward(&index, b);
  b &= b - 1;
  return index;
}

// bを破壊しないpop_lsb()
inline int lsb(uint64_t b) {
  unsigned long index;
  _BitScanForward64(&index, b);
  return index;
}

// 最上位bitのbit位置を得る。0を渡してはならない。
inline int msb(uint64_t b) {
  unsigned long index;
  _BitScanReverse64(&index, b);
  return (int)index;
}

// --------------------
//       置換表
// --------------------

// 局面のハッシュキー
// 盤面(盤上の駒 + 手駒)に対して、Zobrist Hashでそれに対応する値を計算する。
typedef uint64_t Key;

// --------------------
//  指し手オーダリング
// --------------------

struct MovePicker
{
  MovePicker(const Position& pos) {}

  // 次の指し手を得る。指し手がないときは0が返る。
  Move nextMove() { return Move(); }
};

// --------------------
//        探索
// --------------------

namespace Search {

  // 入玉ルール設定
  enum EnteringKingRule
  {
    EKR_NONE ,      // 入玉ルールなし
    EKR_24_POINT,   // 24点法
    EKR_27_POINT,   // 27点法 = CSAルール
    EKR_TRY_RULE,   // トライルール
  };

} // end of namespace Search

// --------------------
//      評価関数
// --------------------

namespace Eval {
  enum BonaPiece : int16_t;

  Value eval(const Position& pos);
}

// --------------------
//     USI関連
// --------------------

namespace USI {
  struct Option;

  // USIのoption名と、それに対応する設定内容を保持しているclass
  typedef std::map<std::string, Option> OptionsMap;

  // USIプロトコルで指定されるoptionの内容を保持するclass
  struct Option {
    typedef void(*OnChange)(const Option&);

    Option(OnChange f = nullptr): type("button"), min(0), max(0), on_change(f) {}

    // bool型のoption デフォルト値が v
    Option(bool v, OnChange f = nullptr) : type("check"),min(0),max(0),on_change(f)
    {  defaultValue = currentValue = v ? "true" : "false";}

    // int型で(min,max)でデフォルトがv
    Option(int v, int min_, int max_, OnChange f = nullptr) : type("spin"),min(min_),max(max_),on_change(f)
    {  defaultValue = currentValue = std::to_string(v); }

    // USIプロトコル経由で値を設定されたときにそれをcurrentValueに反映させる。
    Option& operator=(const std::string&);

    // 起動時に設定を代入する。
    void operator<<(const Option&);

    // int,bool型への暗黙の変換子
    operator int() const {
      ASSERT_LV1(type == "check" || type == "spin");
      return type == "spin" ? stoi(currentValue) : currentValue == "true";
    }

    // string型への暗黙の変換子
    operator std::string() const { ASSERT_LV1(type == "string");  return currentValue; }

  private:
    friend std::ostream& operator<<(std::ostream& os, const OptionsMap& om);

    size_t idx; // 出力するときの順番。この順番に従ってGUIの設定ダイアログに反映されるので順番重要！

    std::string defaultValue, currentValue, type;

    int min, max; // int型のときの最小と最大

    OnChange on_change; // 値が変わったときに呼び出されるハンドラ
  };

  // USIメッセージ応答部(起動時に、各種初期化のあとに呼び出される)
  void loop();

  // optionのdefault値を設定する。
  void init(OptionsMap&);

  // USIプロトコルで、idxの順番でoptionを出力する。
  std::ostream& operator<<(std::ostream& os, const OptionsMap& om);
}

// USIのoption設定はここに保持されている。
extern USI::OptionsMap Options;

// 局面posとUSIプロトコルによる指し手を与えて
// もし可能なら等価で合法な指し手を返す。(合法でないときはMOVE_NONEを返す)
Move move_from_usi(const Position& pos, const std::string& str);

// --------------------
//       misc
// --------------------

// --- enum用のマクロ

// +,-,*など標準的なoperatorを標準的な方法で定義するためのマクロ
// enumで定義されている型に対して用いる。Stockfishのアイデア。

#define ENABLE_OPERATORS_ON(T)                                                  \
  inline T operator+(const T d1, const T d2) { return T(int(d1) + int(d2)); }   \
  inline T operator-(const T d1, const T d2) { return T(int(d1) - int(d2)); }   \
  inline T operator*(const int i, const T d) { return T(i * int(d)); }          \
  inline T operator*(const T d, const int i) { return T(int(d) * i); }          \
  inline T operator-(const T d) { return T(-int(d)); }                          \
  inline T& operator+=(T& d1, const T d2) { return d1 = d1 + d2; }              \
  inline T& operator-=(T& d1, const T d2) { return d1 = d1 - d2; }              \
  inline T& operator*=(T& d, const int i) { return d = T(int(d) * i); }         \
  inline T& operator++(T& d) { return d = T(int(d) + 1); }                      \
  inline T& operator--(T& d) { return d = T(int(d) - 1); }                      \
  inline T operator++(T& d,int) { T prev = d; d = T(int(d) + 1); return prev; } \
  inline T operator--(T& d,int) { T prev = d; d = T(int(d) - 1); return prev; } \
  inline T operator/(const T d, const int i) { return T(int(d) / i); }          \
  inline T& operator/=(T& d, const int i) { return d = T(int(d) / i); }

ENABLE_OPERATORS_ON(Color)
ENABLE_OPERATORS_ON(File)
ENABLE_OPERATORS_ON(Rank)
ENABLE_OPERATORS_ON(Square)
ENABLE_OPERATORS_ON(Piece)
ENABLE_OPERATORS_ON(PieceNo)
ENABLE_OPERATORS_ON(Value)
ENABLE_OPERATORS_ON(Depth)
ENABLE_OPERATORS_ON(Hand)
ENABLE_OPERATORS_ON(HandKind)
ENABLE_OPERATORS_ON(Eval::BonaPiece)

// enumに対してrange forで回せるようにするためのhack(速度低下があるかも知れないので速度の要求されるところでは使わないこと)
#define ENABLE_RANGE_OPERATORS_ON(X,ZERO,NB)     \
  inline X operator*(X x) { return x; }          \
  inline X begin(X x) { return ZERO; }           \
  inline X end(X x) { return NB; }

ENABLE_RANGE_OPERATORS_ON(Square,SQ_ZERO,SQ_NB)
ENABLE_RANGE_OPERATORS_ON(Color, COLOR_ZERO, COLOR_NB)
ENABLE_RANGE_OPERATORS_ON(File,FILE_ZERO,FILE_NB)
ENABLE_RANGE_OPERATORS_ON(Rank,RANK_ZERO,RANK_NB)
ENABLE_RANGE_OPERATORS_ON(Piece, NO_PIECE, PIECE_NB)

// for(auto sq : Square())ではなく、for(auto sq : SQ) のように書くためのhack
#define SQ Square()
#define COLOR Color()
#define FILE File()
#define RANK Rank()
#define PIECE Piece()

// --- N回ループを展開するためのマクロ
// AperyのUnrollerのtemplateによる実装は模範的なコードなのだが、lambdaで書くと最適化されないケースがあったのでマクロで書く。

#define UNROLLER1(Statement_) { const int i = 0; Statement_; }
#define UNROLLER2(Statement_) { UNROLLER1(Statement_); const int i = 1; Statement_;}
#define UNROLLER3(Statement_) { UNROLLER2(Statement_); const int i = 2; Statement_;}
#define UNROLLER4(Statement_) { UNROLLER3(Statement_); const int i = 3; Statement_;}
#define UNROLLER5(Statement_) { UNROLLER4(Statement_); const int i = 4; Statement_;}
#define UNROLLER6(Statement_) { UNROLLER5(Statement_); const int i = 5; Statement_;}

// --- bitboardに対するforeach

// Bitboardのそれぞれの升に対して処理を行なうためのマクロ。
// p[0]側とp[1]側との両方で同じコードが生成されるので生成されるコードサイズに注意。

#define FOREACH_BB(BB_, SQ_, Statement_)					\
	do {										      \
		while (BB_.p[0]) {					\
			SQ_ = BB_.pop_from_p0();	\
			Statement_;								\
		}										        \
		while (BB_.p[1]) {					\
			SQ_ = BB_.pop_from_p1();	\
			Statement_;								\
		}										        \
	} while (false)

#endif // of #ifndef _SHOGI_H_
