#pragma once

#include <vector>
#include <random>
#include "DiceBattleGame.h"

enum class GameRuleMode
{
    Original,
    Advance
};

class DiceBattleAI
{
public:
    struct MoveOption
    {
        int fromX, fromY;
        int toX, toY;
        int score;
    };

    DiceBattleAI() = default;
    ~DiceBattleAI() = default;

    void Update(DiceBattleGame& game, float dt);

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

    void SetSearchDepth(int depth)
    {
        m_searchDepth = depth;
    }
    int GetSearchDepth() const
    {
        return m_searchDepth;
    }

private:
    struct SimPiece
    {
        int gridX, gridY;
        PlayerType owner;
        PieceType type;
        int face;
        bool isActive;

        bool isFrozen = false;
    };

    struct SimBoard
    {
        std::vector<SimPiece> pieces;
        bool isGameOver = false;
        bool isDraw = false; // ★ 引き分けフラグ
        PlayerType winner;

        bool p1Shielded = false;
        bool p2Shielded = false;

        PlayerType currentTurn = PlayerType::Player2;
        int remainingSteps = 1;
    };

    bool ExecuteMove(DiceBattleGame& game);

    int AlphaBeta(SimBoard board, int depth, int alpha, int beta, bool isMaximizing);

    int EvaluateBoard(const SimBoard& board);
    std::vector<MoveOption> GenerateValidMoves(const SimBoard& board, PlayerType player);
    SimBoard ApplyMove(const SimBoard& board, const MoveOption& move, PlayerType player);

    int EvaluateOriginal(const SimBoard& board);
    int EvaluateAdvance(const SimBoard& board);

    GameRuleMode m_ruleMode = GameRuleMode::Original;
    float m_thinkTimer = 0.0f;
    float m_thinkDelay = 1.0f;
    int m_searchDepth = 5;
    std::mt19937 m_rng{std::random_device{}()};
};