#pragma once

#include <vector>
#include <random>
#include "DiceBattleGame.h"

// ゲームルールモード（クラシック / スキルありアドバンス）
enum class GameRuleMode
{
    Original,
    Advance
};

// ============================================================================
// Alpha-Beta探索（ミニマックス法）に基づくCPU思考AIクラス
// 盤面評価関数、将来のサイコロ出目シミュレーションを備えます
// ============================================================================
class DiceBattleAI
{
public:
    // 指し手候補の構造体
    struct MoveOption
    {
        int fromX, fromY; // 移動元
        int toX, toY;     // 移動先
        int score;        // 移動評価値
    };

    DiceBattleAI() = default;
    ~DiceBattleAI() = default;

    // AIの思考および手番実行（タイマー制御付き）
    void Update(DiceBattleGame& game, float dt);

    // 思考ウェイト時間（秒）の設定
    void SetThinkDelay(float seconds)
    {
        m_thinkDelay = seconds;
    }
    void SetRuleMode(GameRuleMode mode)
    {
        m_ruleMode = mode;
    }
    GameRuleMode GetRuleMode() const
    {
        return m_ruleMode;
    }

    // Alpha-Beta探索の深さ設定
    void SetSearchDepth(int depth)
    {
        m_searchDepth = depth;
    }
    int GetSearchDepth() const
    {
        return m_searchDepth;
    }

private:
    // シミュレーション用のコマ情報
    struct SimPiece
    {
        int gridX, gridY;
        PlayerType owner;
        PieceType type;
        int face;
        bool isActive;

        bool isFrozen = false; // 凍結状態
    };

    // シミュレーション用の盤面状態
    struct SimBoard
    {
        std::vector<SimPiece> pieces;
        bool isGameOver = false;
        bool isDraw = false; // 引き分けフラグ[cite: 40]
        PlayerType winner;

        bool p1Shielded = false;
        bool p2Shielded = false;

        PlayerType currentTurn = PlayerType::Player2;
        int remainingSteps = 1;
    };

    // 最適手を決定してゲームロジックへ反映
    bool ExecuteMove(DiceBattleGame& game);

    // Alpha-Beta法による盤面探索の再帰関数
    int AlphaBeta(SimBoard board, int depth, int alpha, int beta, bool isMaximizing);

    // 盤面全体の総合評価値を算出
    int EvaluateBoard(const SimBoard& board);
    // 候補となる合法手を生成
    std::vector<MoveOption> GenerateValidMoves(const SimBoard& board, PlayerType player);
    // 仮想盤面に指し手を適用した後の新状態を返す
    SimBoard ApplyMove(const SimBoard& board, const MoveOption& move, PlayerType player);

    // モード別評価関数
    int EvaluateOriginal(const SimBoard& board);
    int EvaluateAdvance(const SimBoard& board);

    GameRuleMode m_ruleMode = GameRuleMode::Original;
    float m_thinkTimer = 0.0f;
    float m_thinkDelay = 1.0f;
    int m_searchDepth = 5;
    std::mt19937 m_rng{std::random_device{}()}; // 同点候補のランダム選出用
};