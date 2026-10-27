#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <memory>
#include <vector>
#include <random>
#include <unordered_map>
#include <CommonStates.h>
#include "Dice/Dice.h"
#include "Dice/StepDice.h"
#include "Network/NetworkPackets.h"

// クォータニオン回転姿勢から上面の出目（1〜6）を判別する関数
int GetFaceFromRotation(const DirectX::SimpleMath::Quaternion& rot);

enum class PlayerType
{
    Player1,
    Player2
};

enum class PieceType
{
    King,
    Pawn
};

// 盤面上のコマ構造体
struct BoardPiece
{
    PlayerType owner;
    PieceType type;
    int gridX;
    int gridY;
    std::shared_ptr<Dice> diceInstance;

    bool isActive = true;
    int initialX = 0;
    int initialY = 0;

    DirectX::SimpleMath::Vector3 startPos = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 targetPos = DirectX::SimpleMath::Vector3::Zero;

    DirectX::SimpleMath::Quaternion startRot = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Quaternion targetRot = DirectX::SimpleMath::Quaternion::Identity;
    float rotProgress = 1.0f;

    float dropProgress = 1.0f;
    float dropHeight = 15.0f;
    DirectX::SimpleMath::Vector3 randomRotAxis = DirectX::SimpleMath::Vector3::UnitY;
    float dropDuration = 1.2f;
};

// ============================================================================
// ダイスバトルのゲームルール・状態遷移・勝敗判定・全コマ管理メインモデル
// ============================================================================
class DiceBattleGame
{
public:
    static constexpr int BOARD_WIDTH = 9;
    static constexpr int BOARD_HEIGHT = 8;
    static constexpr float ROTATION_SPEED = 5.0f;
    static constexpr float ROLL_ANIM_DURATION = 1.0f;

    DiceBattleGame() = default;
    ~DiceBattleGame() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& proj);

    // ダイスを振ってターン開始
    void StartTurn();
    void StartTurnWithSteps(int fixedSteps);

    // 出目を固定して即時セット
    void SetDiceResultImmediately(int steps);

    // ネットワーク同期パケットの生成と適用
    void BuildWorldSyncPacket(MsgWorldSync& outPacket) const;
    void ApplyWorldSyncFromHost(const MsgWorldSync& syncData);

    // コマの移動実行（対戦バトル発生を含む）
    bool MovePiece(int fromX, int fromY, int toX, int toY);

    // ターンの強制終了
    void ForceEndTurn()
    {
        m_remainingSteps = 0;
        m_hasRolledThisTurn = false;
        m_currentTurn = (m_currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
    }

    void SetRemainingSteps(int steps)
    {
        m_remainingSteps = steps;
    }
    void SetPlayerShield(PlayerType player, bool isShielded)
    {
        m_isShielded[player] = isShielded;
    }
    bool IsPlayerShielded(PlayerType player) const
    {
        auto it = m_isShielded.find(player);
        return (it != m_isShielded.end()) ? it->second : false;
    }

    bool IsAdvanceMode() const
    {
        return m_isAdvanceMode;
    }
    void SetAdvanceMode(bool isAdvance)
    {
        m_isAdvanceMode = isAdvance;
    }

    bool HasDeadPiece(PlayerType player) const;
    bool RevivePieceAt(PlayerType player, int targetX, int targetY);

    const StepDice* GetStepDiceInstance() const
    {
        return &m_stepDice;
    }

    // 移動中またはアニメーション中のコマが存在するか
    bool IsAnyPieceMoving() const
    {
        for (const auto& piece : m_pieces)
        {
            if (!piece.isActive)
                continue;
            if (piece.rotProgress < 1.0f || piece.dropProgress < 1.0f)
                return true;
        }
        return false;
    }

    // --- 状態ゲッター ---
    PlayerType GetCurrentTurn() const
    {
        return m_currentTurn;
    }
    int GetRemainingSteps() const
    {
        return m_remainingSteps;
    }
    bool IsWaitingForRoll() const
    {
        return m_isWaitingForRoll;
    }
    bool IsGameOver() const
    {
        return m_isGameOver;
    }
    bool IsDraw() const
    {
        return m_isDraw;
    }
    PlayerType GetWinner() const
    {
        return m_winner;
    }
    const std::vector<BoardPiece>& GetPieces() const
    {
        return m_pieces;
    }

    const BoardPiece* GetPieceAt(int x, int y) const;
    BoardPiece* GetPieceAt(int x, int y);

    int GetPieceTopFace(const BoardPiece* piece) const
    {
        if (!piece || !piece->diceInstance)
            return 1;
        return GetFaceFromRotation(piece->targetRot);
    }

    int GetJustLandedSteps() const
    {
        return m_justLandedSteps;
    }
    void ClearJustLandedSteps()
    {
        m_justLandedSteps = 0;
    }

    DirectX::SimpleMath::Vector3 GridToWorldPosition(int gridX, int gridY) const;

    void SetCursorPosition(int x, int y)
    {
        m_cursorX = x;
        m_cursorY = y;
    }

    int GetBoardWidth() const
    {
        return m_boardWidth;
    }
    int GetBoardHeight() const
    {
        return m_boardHeight;
    }

    void ResizeBoard(int newWidth, int newHeight)
    {
        m_boardWidth = newWidth;
        m_boardHeight = newHeight;
        if (m_device && m_context)
        {
            Initialize(m_device, m_context);
        }
    }

    void RemovePieceAt(int x, int y);
    void AddPieceAt(int x, int y, PlayerType owner, PieceType type, int topValue);

private:
    void ResolveBattle(BoardPiece& attacker, BoardPiece& defender, int targetX, int targetY);
    void CheckGameEndConditions();

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    int m_boardWidth = BOARD_WIDTH;
    int m_boardHeight = BOARD_HEIGHT;

    std::vector<BoardPiece> m_pieces;
    StepDice m_stepDice; // 歩数決定用ダイス

    PlayerType m_currentTurn = PlayerType::Player1;
    int m_remainingSteps = 0;
    bool m_isWaitingForRoll = false;
    bool m_hasRolledThisTurn = false; // このターンでダイスを振ったかのフラグ[cite: 41]

    float m_rollAnimTimer = 0.0f;

    int m_targetFixedSteps = 0;
    int m_justLandedSteps = 0;

    DirectX::SimpleMath::Vector3 m_throwStartPos;
    DirectX::SimpleMath::Vector3 m_throwTargetPos;

    bool m_isGameOver = false;
    bool m_isDraw = false;
    PlayerType m_winner = PlayerType::Player1;

    std::unordered_map<PlayerType, bool> m_isShielded;

    bool m_isAdvanceMode = true;

    int m_cursorX = 0;
    int m_cursorY = 0;
};