#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <memory>
#include <vector>
#include <random>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <CommonStates.h>
#include "Dice/Dice.h"
#include "Dice/StepDice.h"
#include "Network/NetworkPackets.h"

// ゲーム設定定数定義
namespace DiceBattleConstants
{
    constexpr int DEFAULT_BOARD_WIDTH = 9;             // 盤面の横幅マス数
    constexpr int DEFAULT_BOARD_HEIGHT = 9;            // 盤面の縦幅マス数
    constexpr int MAX_TURNS = 20;                      // 1ゲームの最大手数（先手・後手の両方が動かしきって1手）
    constexpr float ROTATION_SPEED = 5.0f;             // サイコロ転がり回転補間スピード
    constexpr float ROLL_ANIM_DURATION = 1.0f;         // 出目決定ダイス投入アニメーション時間（秒）
    constexpr float CELL_SIZE = 2.0f;                  // 1マスのワールドサイズ
    constexpr float STEP_DICE_THROW_ARC_HEIGHT = 3.5f; // ダイス投入時の放物線高さ
    constexpr float DICE_SE_VOLUME_NORMAL = 2.0f;      // 通常ダイス音量
    constexpr float DICE_SE_VOLUME_LAND = 5.0f;        // 着地ダイス音量
}

// クォータニオン回転姿勢から上面の出目（1〜6）を判別する関数
int GetFaceFromRotation(const DirectX::SimpleMath::Quaternion& rot);

// 対戦プレイヤー種別
enum class PlayerType
{
    Player1, // 1P（青側プレイヤー）
    Player2  // 2P（赤側プレイヤー / AI）
};

// マスの塗りカラー種別
enum class TileColor
{
    None = 0, // 未塗装
    Player1,  // 1Pカラー（青）
    Player2   // 2Pカラー（赤）
};

// コマ種別
enum class PieceType
{
    King, // 王（勝利条件ターゲット）
    Pawn  // 兵（サイコロ）
};

// 盤面上のコマ構造体
struct BoardPiece
{
    PlayerType owner;                   // 所有プレイヤー
    PieceType type;                     // コマ種別
    int gridX;                          // 格子X座標
    int gridY;                          // 格子Y座標
    std::shared_ptr<Dice> diceInstance; // 3Dダイスモデルインスタンス参照

    bool isActive = true; // 生存フラグ
    int initialX = 0;     // 初期配置X座標
    int initialY = 0;     // 初期配置Y座標

    DirectX::SimpleMath::Vector3 startPos = DirectX::SimpleMath::Vector3::Zero;  // 移動アニメーション開始位置
    DirectX::SimpleMath::Vector3 targetPos = DirectX::SimpleMath::Vector3::Zero; // 移動アニメーション目標位置

    DirectX::SimpleMath::Quaternion startRot = DirectX::SimpleMath::Quaternion::Identity;  // 回転アニメーション開始姿態
    DirectX::SimpleMath::Quaternion targetRot = DirectX::SimpleMath::Quaternion::Identity; // 回転アニメーション目標姿態
    float rotProgress = 1.0f;                                                              // 補間進行度 (0.0～1.0)

    float dropProgress = 1.0f;                                                        // 落下アニメーション進行度
    float dropHeight = 15.0f;                                                         // 落下アニメーション開始高さ
    DirectX::SimpleMath::Vector3 randomRotAxis = DirectX::SimpleMath::Vector3::UnitY; // 落下時ランダム回転軸
    float dropDuration = 1.2f;                                                        // 落下アニメーション時間
};

// ダイスバトルのゲームルール・状態遷移・勝敗判定・全コマ管理メインモデル
class DiceBattleGame
{
public:
    static constexpr int BOARD_WIDTH = DiceBattleConstants::DEFAULT_BOARD_WIDTH;
    static constexpr int BOARD_HEIGHT = DiceBattleConstants::DEFAULT_BOARD_HEIGHT;

    using DefeatedCallback =
        std::function<void(const DirectX::SimpleMath::Vector3& attackerPos,
                           const DirectX::SimpleMath::Vector3& victimPos, const std::string& modelPath)>;

    DiceBattleGame() = default;
    ~DiceBattleGame() = default;

    void SetOnPieceDefeatedCallback(DefeatedCallback cb)
    {
        m_onPieceDefeated = cb;
    }

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& proj);

    void StartTurn();
    void StartTurnWithSteps(int fixedSteps);
    void SetDiceResultImmediately(int steps);

    int GetTargetFixedSteps() const
    {
        return m_targetFixedSteps;
    }

    // ダイスの振出フラグおよび残り歩数を初期化し、同じターン内でのサイコロ再ロール処理を可能にする関数
    void ResetDiceRollState()
    {
        m_isWaitingForRoll = false;
        m_hasRolledThisTurn = false;
        m_remainingSteps = 0;
    }

    // 通信で連続パケットが届いた際に移動中アニメーションを即座に完了位置へ確定させる関数
    void FinishAllPieceAnimations()
    {
        for (auto& piece : m_pieces)
        {
            if (!piece.isActive)
                continue;
            if (piece.rotProgress < 1.0f)
            {
                piece.rotProgress = 1.0f;
                if (piece.diceInstance)
                {
                    piece.diceInstance->SetRotation(piece.targetRot);
                    piece.diceInstance->SetPosition(piece.targetPos);
                }
            }
        }
    }

    void BuildWorldSyncPacket(MsgWorldSync& outPacket) const;
    void ApplyWorldSyncFromHost(const MsgWorldSync& syncData);

    bool MovePiece(int fromX, int fromY, int toX, int toY);

    void ForceEndTurn()
    {
        m_remainingSteps = 0;
        AdvanceTurnAndCheckLimit();
    }

    // 先攻決定ダイスで決まったプレイヤーからゲームを開始する関数
    void SetFirstTurnPlayer(PlayerType firstPlayer)
    {
        m_firstTurnPlayer = firstPlayer;
        m_currentTurn = firstPlayer;
        m_turnCount = 0;
        m_remainingSteps = 0;
        m_hasRolledThisTurn = false;
        m_isWaitingForRoll = false;
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

    // --- 陣地塗り・20ターン制ルール用アクセサ ---

    // 指定マスの塗りカラーを取得
    TileColor GetTileColor(int x, int y) const
    {
        if (y < 0 || y >= static_cast<int>(m_tileColors.size()))
            return TileColor::None;
        if (x < 0 || x >= static_cast<int>(m_tileColors[y].size()))
            return TileColor::None;
        return m_tileColors[y][x];
    }

    // 指定プレイヤーの現在の塗りマス数を集計
    int GetPaintedTileCount(PlayerType player) const
    {
        TileColor target = (player == PlayerType::Player1) ? TileColor::Player1 : TileColor::Player2;
        int count = 0;
        for (const auto& row : m_tileColors)
        {
            for (TileColor cell : row)
            {
                if (cell == target)
                    count++;
            }
        }
        return count;
    }

    // 現在の完了手数（0〜20）を取得
    int GetCurrentTurnCount() const
    {
        return m_turnCount;
    }

    // 残り手数（20〜0）を取得
    int GetRemainingTurns() const
    {
        return std::max(0, DiceBattleConstants::MAX_TURNS - m_turnCount);
    }

    // 盤面全体の塗りカラー配列を取得（GridField::Render 連携用）
    const std::vector<std::vector<TileColor>>& GetTileColors() const
    {
        return m_tileColors;
    }

private:
    void ResolveBattle(BoardPiece& attacker, BoardPiece& defender, int targetX, int targetY);
    void CheckGameEndConditions();

    // 指定マスをプレイヤーのカラーで塗る（他カラーの上書き可）
    void PaintTile(int x, int y, PlayerType owner);

    // ターン交代と20手到達時の強制終了・塗りマス数勝敗判定
    void AdvanceTurnAndCheckLimit();

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    int m_boardWidth = DiceBattleConstants::DEFAULT_BOARD_WIDTH;
    int m_boardHeight = DiceBattleConstants::DEFAULT_BOARD_HEIGHT;

    std::vector<BoardPiece> m_pieces;
    std::vector<std::vector<TileColor>> m_tileColors; // 各マスの陣地カラー
    StepDice m_stepDice;

    PlayerType m_firstTurnPlayer = PlayerType::Player1; // 先攻プレイヤー
    PlayerType m_currentTurn = PlayerType::Player1;
    int m_turnCount = 0; // 先攻・後手の両方が動かしきった手数（20で強制終了）
    int m_remainingSteps = 0;
    bool m_isWaitingForRoll = false;
    bool m_hasRolledThisTurn = false;

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

    DefeatedCallback m_onPieceDefeated;
};