#include <sstream>

#include "shogi.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "misc.h"

using namespace std;

// Positionクラスがそこにいたるまでの手順(捕獲された駒など)を保持しておかないと千日手判定が出来ないので
// StateInfo型のstackのようなものが必要となるので、それをglobalに確保しておく。
Search::StateStackPtr SetupStates;

// ユーザーの実験用に開放している関数。
// USI拡張コマンドで"user"と入力するとこの関数が呼び出される。
// "user"コマンドの後続に指定されている文字列はisのほうに渡される。
extern void user_test(Position& pos, std::istringstream& is);

// USI拡張コマンドの"test"コマンド。
// サンプル用のコードを含めてtest.cppのほうに色々書いてあるのでそれを呼び出すために使う。
#ifdef ENABLE_TEST_CMD
extern void test_cmd(Position& pos, istringstream& is);
extern void perft(Position& pos, istringstream& is);
extern void generate_moves_cmd(Position& pos);
#endif

// 協力詰めsolverモード
#ifdef    COOPERATIVE_MATE_SOLVER
#include "extra/cooperative_mate_solver.h"
#endif

// --------------------
//     USI::Option
// --------------------

// Option設定が格納されたglobal object。
USI::OptionsMap Options;

namespace USI
{
  // この関数はUSI::init()から起動時に呼び出されるだけ。
  void Option::operator<<(const Option& o)
  {
    static size_t insert_order = 0;
    *this = o;
    idx = insert_order++; // idxは0から連番で番号を振る
  }

  // optionのdefault値を設定する。
  void init(OptionsMap& o)
  {
    // Hash上限。32bitモードなら2GB、64bitモードなら1024GB
    const int MaxHashMB = Is64Bit ? 1024 * 1024 : 2048;

    o["Threads"] << Option(4, 1, 128, [](auto& o) { Threads.read_usi_options(); });

    // USIプロトコルでは、"USI_Hash"と"USI_Ponder"なのだが、
    // 置換表サイズを変更しての自己対戦などをさせたいので、
    // 片方だけ変更できなければならない。
    // ゆえにGUIでの対局設定は無視して、思考エンジンの設定ダイアログのところで
    // 個別設定が出来るようにする。
    o["Hash"]    << Option(16, 1, MaxHashMB, [](auto&o) { TT.resize(o); });
    o["Ponder"]  << Option(false);

    // 協力詰めsolver
#ifdef    COOPERATIVE_MATE_SOLVER
    o["CM_Hash"] << Option(16, 1, MaxHashMB, [](auto&o) { CooperativeMate::TT.resize(o); });
#endif

    // cin/coutの入出力をファイルにリダイレクトする
    o["WriteDebugLog"] << Option(false, [](auto& o) { start_logger(o); });

  }

  // USIプロトコル経由で値を設定されたときにそれをcurrentValueに反映させる。
  Option& Option::operator=(const string& v) {

    ASSERT_LV1(!type.empty());

    // 範囲外
    if ((type != "button" && v.empty())
      || (type == "check" && v != "true" && v != "false")
      || (type == "spin" && (stoi(v) < min || stoi(v) > max)))
      return *this;

    // ボタン型は値を設定するものではなく、単なるトリガーボタン。
    // ボタン型以外なら入力値をcurrentValueに反映させてやる。
    if (type != "button")
      currentValue = v;

    // 値が変化したのでハンドラを呼びだす。
    if (on_change)
      on_change(*this);

    return *this;
  }

  std::ostream& operator<<(std::ostream& os, const OptionsMap& om)
  {
    // idxの順番を守って出力する
    for (size_t idx = 0; idx < om.size();++idx)
      for(const auto& it:om)
        if (it.second.idx == idx)
        {
          const Option& o = it.second;
          os << "option name " << it.first << " type " << o.type;
          if (o.type != "button")
            os << " default " << o.defaultValue;
          if (o.type == "spin")
            os << " min " << o.min << " max " << o.max;
          os << endl;
          break;
        }

    return os;
  }
}

// --------------------
// USI関係のコマンド処理
// --------------------

// isreadyコマンド処理部
void is_ready_cmd()
{
  static bool first = true;

  // 評価関数の読み込みなど時間のかかるであろう処理はこのタイミングで行なう。
  // 起動時に時間のかかる処理をしてしまうと将棋所がタイムアウト判定をして、思考エンジンとしての認識をリタイアしてしまう。
  if (first)
  {
    // 評価関数の読み込み
    Eval::load_eval();

    first = false;
  }

  Search::clear();

  cout << "readyok" << endl;
}

// "position"コマンド処理部
void position_cmd(Position& pos,istringstream& is)
{
  Move m;
  string token, sfen;

  is >> token;

  if (token == "startpos")
  {
    // 初期局面として初期局面のFEN形式の入力が与えられたとみなして処理する。
    sfen = SFEN_HIRATE;
    is >> token; // もしあるなら"moves"トークンを消費する。
  }
  // 局面がfen形式で指定されているなら、その局面を読み込む。
  // UCI(チェスプロトコル)ではなくUSI(将棋用プロトコル)だとここの文字列は"fen"ではなく"sfen"
  else if (token == "sfen")
    while (is >> token && token != "moves")
      sfen += token + " ";
  else
    return;

  pos.set(sfen);

  SetupStates = Search::StateStackPtr(new std::stack<StateInfo>);

  // 指し手のリストをパースする(あるなら)
  while (is >> token && (m = move_from_usi(pos, token)) != MOVE_NONE)
  {
    // 1手進めるごとにStateInfoが積まれていく。これは千日手の検出のために必要。
    // ToDoあとで考える。
    SetupStates->push(StateInfo());
    pos.do_move(m, SetupStates->top());
  }
}

// "setoption"コマンド応答。
void setoption_cmd(istringstream& is)
{
  string token, name, value;

  while (is >> token && token != "value")
    // "name"トークンはあってもなくても良いものとする。(手打ちでコマンドを打つときには省略したい)
    if (token != "name")
      // スペース区切りで長い名前のoptionを使うことがあるので2つ目以降はスペースを入れてやる
      name += (name.empty() ? "" : " ") + token;

  // valueの後ろ。スペース区切りで複数文字列が来ることがある。
  while (is >> token)
    value +=  (value.empty() ? "" : " ") + token;

  if (Options.count(name))
    Options[name] = value;
  else {
    // USI_HashとUSI_Ponderは無視してやる。
    if (name != "USI_Hash" && name != "USI_Ponder" )
      // この名前のoptionは存在しなかった
      sync_cout << "No such option: " << name << sync_endl;
  }
}

// go()は、思考エンジンがUSIコマンドの"go"を受け取ったときに呼び出される。
// この関数は、入力文字列から思考時間とその他のパラメーターをセットし、探索を開始する。
void go_cmd(const Position& pos, istringstream& is) {

  Search::LimitsType limits;
  string token;

  while (is >> token)
  {
    // 探索すべき指し手。(探索開始局面から特定の初手だけ探索させるとき)
    if (token == "searchmoves")
      // 残りの指し手すべてをsearchMovesに突っ込む。
      while (is >> token)
        limits.searchmoves.push_back(move_from_usi(pos, token));

    // 先手、後手の残り時間。[ms]
    else if (token == "wtime")     is >> limits.time[WHITE];
    else if (token == "btime")     is >> limits.time[BLACK];

    // 秒読み設定。
    else if (token == "byoyomi") {
      int t = 0;
      is >> t;

      // USIプロトコルで送られてきた秒読み時間より少なめに思考する設定
      // ※　通信ラグがあるときに、ここで少なめに思考しないとタイムアップになる可能性があるので。

      // t = std::max(t - Options["ByoyomiMinus"], Time::point(0));

      // USIプロトコルでは、これが先手後手同じ値だと解釈する。
      limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = t;
    }
    // この探索深さで探索を打ち切る
    else if (token == "depth")     is >> limits.depth;

    // この探索ノード数で探索を打ち切る
    else if (token == "nodes")     is >> limits.nodes;

    // 詰み探索。"UCI"プロトコルではこのあとには手数が入っており、その手数以内に詰むかどうかを判定するが、
    // "USI"プロトコルでは、ここは探索のための時間制限に変更となっている。
    else if (token == "mate") {
      is >> token;
      if (token == "infinite")
        limits.mate = INT32_MAX;
      else
        is >> limits.mate;
    }

    // 時間無制限。
    else if (token == "infinite")  limits.infinite = 1;

    // ponderモードでの思考。
    else if (token == "ponder")    limits.ponder = 1;
  }

  Threads.start_thinking(pos, limits, Search::SetupStates);
}



// --------------------
// 　　USI応答部
// --------------------

// USI応答部本体
void USI::loop()
{
  // 探索開始局面(root)を格納するPositionクラス
  Position pos;

  string cmd,token;
  while (getline(cin, cmd))
  {
    istringstream is(cmd);

    token = "";
    is >> skipws >> token;

    if (token == "quit" || token == "stop")
    {
      Search::Signals.stop = true;
      Threads.main()->notify_one(); // main threadに受理させる
    }

    // 与えられた局面について思考するコマンド
    else if (token == "go") go_cmd(pos, is);

    // (思考などに使うための)開始局面(root)を設定する
    else if (token == "position") position_cmd(pos, is);

    // 起動時いきなりこれが飛んでくるので速攻応答しないとタイムアウトになる。
    else if (token == "usi")
      sync_cout << "id name " << engine_info() << Options << "usiok" << sync_endl;

    // オプションを設定する
    else if (token == "setoption") setoption_cmd(is);

    // 思考エンジンの準備が出来たかの確認
    else if (token == "isready") is_ready_cmd();

    // ユーザーによる実験用コマンド。user.cppのuser()が呼び出される。
    else if (token == "user") user_test(pos,is);

    // 現在の局面を表示する。(デバッグ用)
    else if (token == "d") cout << pos << endl;

    // 指し手生成のテスト
    else if (token == "s") generate_moves_cmd(pos);

    // 指し手生成祭りの局面をセットする。
    else if (token == "matsuri") pos.set("l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w GR5pnsg 1");

    // 現在の局面のsfen文字列が表示される。(デバッグ用)
    else if (token == "sfen") cout << pos.sfen() << endl;

    // ログファイルの書き出しのon
    else if (token == "log") start_logger(true);

    // 現在の局面について評価関数を呼び出して、その値を返す。
    else if (token == "eval") cout << "eval = " << Eval::eval(pos) << endl;

    // この局面での指し手をすべて出力
    else if (token == "moves") {
      for (auto m : MoveList<LEGAL_ALL>(pos))
        cout << m.move << ' ';
      cout << endl;
    }

    // この局面が詰んでいるかの判定
    else if (token == "mated") cout << pos.is_mated() << endl;

    // この局面のhash keyの値を出力
    else if (token == "key") cout << hex << pos.state()->key() << dec << endl;

#ifdef ENABLE_TEST_CMD
    // パフォーマンステスト(Stockfishにある、合法手N手で到達できる局面を求めるやつ)
    else if (token == "perft") perft(pos, is);

    // テストコマンド
    else if (token == "test") test_cmd(pos, is);
#endif

    ;

  }
}

// --------------------
// USI関係の記法変換部
// --------------------

// USIの指し手文字列などに使われている盤上の升を表す文字列をSquare型に変換する
// 変換できなかった場合はSQ_NBが返る。
Square usi_to_sq(char f, char r)
{
  File file = toFile(f);
  Rank rank = toRank(r);

  if (is_ok(file) && is_ok(rank))
    return file | rank;

  return SQ_NB;
}

// uciから指し手への変換。本来この関数は要らないのだが、
// 棋譜を大量に読み込む都合、この部分をそこそこ高速化しておきたい。
Move uci_to_move(const string& str)
{
  // さすがに3文字以下の指し手はおかしいだろ。
  if (str.length() <= 3)
    return MOVE_NONE;

  Square to = usi_to_sq(str[2], str[3]);
  if (!is_ok(to))
    return MOVE_NONE;

  bool promote = str.length() == 5 && str[4] == '+';
  bool drop = str[1] == '*';

  Move move = MOVE_NONE;
  if (!drop)
  {
    Square from = usi_to_sq(str[0],str[1]);
    if (is_ok(from))
      move = promote ? make_move_promote(from, to) : make_move(from, to);
  }
  else
  {
    for (int i = 1; i <= 7; ++i)
      if (PieceToCharBW[i] == str[0])
      {
        move = make_move_drop((Piece)i, to);
        break;
      }
  }

  return move;
}


// 局面posとUSIプロトコルによる指し手を与えて
// もし可能なら等価で合法な指し手を返す。(合法でないときはMOVE_NONEを返す)
Move move_from_usi(const Position& pos, const std::string& str)
{
  // 全合法手のなかからuci文字列に変換したときにstrと一致する指し手を探してそれを返す
  //for (const ExtMove& ms : MoveList<LEGAL_ALL>(pos))
  //  if (str == move_to_uci(ms.move))
  //    return ms.move;

  // ↑のコードは大変美しいコードではあるが、棋譜を大量に読み込むときに時間がかかるうるのでもっと高速な実装をする。

  // uci文字列をmoveに変換するやつがいるがな..
  Move move = uci_to_move(str);

  /*
  CheckInfo ci(pos);
  if (pos.pseudo_legal(move) && pos.legal(move, ci.pinned))
    return move;
    */

  if (true)
    return move;

  // いかなる状況であろうとこのような指し手はエラー表示をして弾いていいと思うが…。
  // cout << "\nIlligal Move : " << str << "\n";

  return MOVE_NONE;
}
