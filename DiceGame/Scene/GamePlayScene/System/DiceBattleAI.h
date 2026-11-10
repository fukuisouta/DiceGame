#pragma once

#include <vector>
#include <random>
#include "DiceBattleGame.h"

// AI定数定義
namespace AIConstants
{
    constexpr float DEFAULT_THINK_DELAY = 1.0f; // デフォルトの思考思考ディレイ（秒）
    constexpr int DEFAULT_SEARCH_DEPTH = 5;     // デフォルトのAlpha-Beta探索深さ
}

// ゲームルールモード（クラシック / スキルありアドバンス）
enum class GameRuleMode
{
    Original,
    Advance
};

// Alpha-Beta探索（ミニマックス法）に基づくCPU思考AIクラス
class DiceBattleAI
{
public:
    // 指し手候補の構造体
    struct MoveOption
    {
        int fromX, fromY; // 移動元格子座標
        int toX, toY;     // 移動先格子座標
        int score;        // 移動評価値
    };

    DiceBattleAI() = default;
    ~DiceBattleAI() = default;

    // AIの思考および手番実行（タイマー制御付き）を行う関数
    // 引数 game: ダイスバトルゲームロジック参照, dt: デルタタイム
    void Update(DiceBattleGame& game, float dt);

    // 思考ウェイト時間（秒）を設定する関数
    // 引数 seconds: 思考ディレイ時間
    void SetThinkDelay(float seconds)
    {
        m_thinkDelay = seconds;
    }

    // ルールモードを設定する関数
    // 引数 mode: ゲームルールモード
    void SetRuleMode(GameRuleMode mode)
    {
        m_ruleMode = mode;
    }

    // 現在のルールモードを取得する関数
    // 戻り値: ゲームルールモード
    GameRuleMode GetRuleMode() const
    {
        return m_ruleMode;
    }

    // Alpha-Beta探索の深さを設定する関数
    // 引数 depth: 探索ステップ深さ
    void SetSearchDepth(int depth)
    {
        m_searchDepth = depth;
    }

    // 探索深さを取得する関数
    // 戻り値: 探索深さ
    int GetSearchDepth() const
    {
        return m_searchDepth;
    }

private:
    // シミュレーション用のコマ情報構造体
    struct SimPiece
    {
        int gridX, gridY; // 格子座標
        PlayerType owner; // 所有プレイヤー
        PieceType type;   // コマ種別
        int face;         // 現在の出目
        bool isActive;    // 生存フラグ

        bool isFrozen = false; // 凍結スキルの影響フラグ
    };

    // シミュレーション用の仮想盤面状態構造体
    struct SimBoard
    {
        std::vector<SimPiece> pieces; // 配置コマ配列
        bool isGameOver = false;      // 対局終了フラグ
        bool isDraw = false;          // 引き分けフラグ[cite: 40]
        PlayerType winner;            // 勝者プレイヤー

        bool p1Shielded = false; // 1P守護状態
        bool p2Shielded = false; // 2P守護状態

        PlayerType currentTurn = PlayerType::Player2; // 現在の手番
        int remainingSteps = 1;                       // 残り歩数
    };

    // 最適手を決定して実際のゲームロジックへ反映する内部関数
    // 戻り値: 行動実行に成功した場合 true
    bool ExecuteMove(DiceBattleGame& game);

    // Alpha-Beta法による盤面探索の再帰関数
    // 引数 board: 仮想盤面, depth: 残り深さ, alpha: アルファカット値, beta: ベータカット値, isMaximizing:
    // 最大化フェーズフラグ 戻り値: 評価スコア
    int AlphaBeta(SimBoard board, int depth, int alpha, int beta, bool isMaximizing);

    // 盤面全体の総合評価値を算出する内部関数
    // 戻り値: 算出されたスコア
    int EvaluateBoard(const SimBoard& board);

    // 候補となる合法手を生成する内部関数
    // 引数 board: 仮想盤面, player: 対象プレイヤー
    // 戻り値: 指し手候補のリスト
    std::vector<MoveOption> GenerateValidMoves(const SimBoard& board, PlayerType player);

    // 仮想盤面に指し手を適用した後の新状態を返す内部関数
    // 引数 board: 適用前盤面, move: 指し手, player: 実行プレイヤー
    // 戻り値: 適用後の新仮想盤面
    SimBoard ApplyMove(const SimBoard& board, const MoveOption& move, PlayerType player);

    // オリジナルルールの評価関数
    int EvaluateOriginal(const SimBoard& board);

    // アドバンスルールの評価関数
    int EvaluateAdvance(const SimBoard& board);

private:
    GameRuleMode m_ruleMode = GameRuleMode::Original;      // 現在のゲームモード
    float m_thinkTimer = 0.0f;                             // 思考ディレイ用経過タイマー
    float m_thinkDelay = AIConstants::DEFAULT_THINK_DELAY; // 思考ウェイト設定値（秒）
    int m_searchDepth = AIConstants::DEFAULT_SEARCH_DEPTH; // アルファベータ探索深さ
    std::mt19937 m_rng{std::random_device{}()};            // 同点候補のランダム選出用乱数生成器
};