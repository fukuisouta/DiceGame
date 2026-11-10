#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <memory>
#include <vector>
#include <random>
#include <unordered_map>
#include <functional>
#include <CommonStates.h>
#include "Dice/Dice.h"
#include "Dice/StepDice.h"
#include "Network/NetworkPackets.h"

// ゲーム設定定数定義
namespace DiceBattleConstants
{
    constexpr int DEFAULT_BOARD_WIDTH = 9;             // 盤面の横幅マス数
    constexpr int DEFAULT_BOARD_HEIGHT = 8;            // 盤面の縦幅マス数
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
    Player1, // 1P（下側プレイヤー / ホスト）
    Player2  // 2P（上側プレイヤー / クライアント / AI）
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

    // コマ撃破時のイベント通知コールバック（攻撃者位置, 被弾者位置, モデルパス）
    using DefeatedCallback =
        std::function<void(const DirectX::SimpleMath::Vector3& attackerPos,
                           const DirectX::SimpleMath::Vector3& victimPos, const std::string& modelPath)>;

    DiceBattleGame() = default;
    ~DiceBattleGame() = default;

    // コマ撃破時コールバックを登録する関数
    // 引数 cb: コールバック関数
    void SetOnPieceDefeatedCallback(DefeatedCallback cb)
    {
        m_onPieceDefeated = cb;
    }

    // ゲーム全体の初期化（コマ配置・歩数決定ダイス初期化）を行う関数
    // 引数 device: Direct3D11デバイス, context: デバイスコンテキスト
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // 毎フレームのダイスアニメーションおよびコマ移動進行度を更新する関数
    // 引数 dt: デルタタイム
    void Update(float dt);

    // 歩数ダイスおよび盤面上の全コマモデルを描画する関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, view: ビュー行列, proj: プロジェクション行列
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& proj);

    // ターンを開始して歩数決定ダイスを投入する関数
    void StartTurn();

    // 出目を手動固定してターンを開始する関数（通信受信用）
    // 引数 fixedSteps: 固定出目歩数
    void StartTurnWithSteps(int fixedSteps);

    // 出目を確定して演出なしで即時セットする関数
    // 引数 steps: 確定歩数
    void SetDiceResultImmediately(int steps);

    // 現在のワールド全体姿勢同期パケットを構築する関数
    // 引数 outPacket: 出力先パケット参照
    void BuildWorldSyncPacket(MsgWorldSync& outPacket) const;

    // ホストから受信したワールド全体姿勢を適用する関数
    // 引数 syncData: 受信パケット
    void ApplyWorldSyncFromHost(const MsgWorldSync& syncData);

    // 指定位置から指定位置へコマを1マス移動する関数（戦闘発生を含む）
    // 引数 fromX, fromY: 移動元, toX, toY: 移動先
    // 戻り値: 移動が成立した場合 true
    bool MovePiece(int fromX, int fromY, int toX, int toY);

    // ターンを強制終了して相手のターンへ移行する関数
    void ForceEndTurn()
    {
        m_remainingSteps = 0;
        m_hasRolledThisTurn = false;
        m_currentTurn = (m_currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
    }

    // 残り歩数を設定する関数
    // 引数 steps: 残り歩数
    void SetRemainingSteps(int steps)
    {
        m_remainingSteps = steps;
    }

    // 守護（シールド）状態を設定する関数
    // 引数 player: プレイヤー, isShielded: 有効フラグ
    void SetPlayerShield(PlayerType player, bool isShielded)
    {
        m_isShielded[player] = isShielded;
    }

    // 守護状態を取得する関数
    // 引数 player: プレイヤー
    // 戻り値: 守護中であれば true
    bool IsPlayerShielded(PlayerType player) const
    {
        auto it = m_isShielded.find(player);
        return (it != m_isShielded.end()) ? it->second : false;
    }

    // アドバンスモード中かどうかを取得する関数
    // 戻り値: アドバンスモードであれば true
    bool IsAdvanceMode() const
    {
        return m_isAdvanceMode;
    }

    // アドバンスモードフラグを設定する関数
    // 引数 isAdvance: 設定フラグ
    void SetAdvanceMode(bool isAdvance)
    {
        m_isAdvanceMode = isAdvance;
    }

    // 蘇生可能な撃破済みコマが存在するかどうかを取得する関数
    // 引数 player: プレイヤー
    // 戻り値: 撃破コマが存在する場合 true
    bool HasDeadPiece(PlayerType player) const;

    // 指定マスに撃破済みコマを蘇生配置する関数
    // 引数 player: プレイヤー, targetX, targetY: 配置先
    // 戻り値: 蘇生成功時 true
    bool RevivePieceAt(PlayerType player, int targetX, int targetY);

    // 歩数決定ダイスのインスタンスポインタを取得する関数
    // 戻り値: StepDiceポインタ
    const StepDice* GetStepDiceInstance() const
    {
        return &m_stepDice;
    }

    // 現在移動中または転がりアニメーション中のコマが存在するかを取得する関数
    // 戻り値: アニメーション中であれば true
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

    // 現在の手番プレイヤーを取得する関数
    PlayerType GetCurrentTurn() const
    {
        return m_currentTurn;
    }

    // 現在の残り歩数を取得する関数
    int GetRemainingSteps() const
    {
        return m_remainingSteps;
    }

    // 歩数ダイスの回転結果待ち状態かを取得する関数
    bool IsWaitingForRoll() const
    {
        return m_isWaitingForRoll;
    }

    // ゲームオーバー判定を取得する関数
    bool IsGameOver() const
    {
        return m_isGameOver;
    }

    // 引き分け判定を取得する関数
    bool IsDraw() const
    {
        return m_isDraw;
    }

    // 勝者プレイヤーを取得する関数
    PlayerType GetWinner() const
    {
        return m_winner;
    }

    // 全コマ配列を取得する関数
    const std::vector<BoardPiece>& GetPieces() const
    {
        return m_pieces;
    }

    // 指定格子座標にあるコマのポインタを取得する関数
    const BoardPiece* GetPieceAt(int x, int y) const;
    BoardPiece* GetPieceAt(int x, int y);

    // 指定コマの現在の上面出目を判定取得する関数
    // 引数 piece: コマポインタ
    // 戻り値: 出目数字 (1～6)
    int GetPieceTopFace(const BoardPiece* piece) const
    {
        if (!piece || !piece->diceInstance)
            return 1;
        return GetFaceFromRotation(piece->targetRot);
    }

    // 直前に入目確定した歩数を取得する関数
    int GetJustLandedSteps() const
    {
        return m_justLandedSteps;
    }

    // 直前の確定歩数をクリアする関数
    void ClearJustLandedSteps()
    {
        m_justLandedSteps = 0;
    }

    // 格子座標からワールド3D位置座標を算出する関数
    // 引数 gridX, gridY: 格子座標
    // 戻り値: 3Dワールド座標
    DirectX::SimpleMath::Vector3 GridToWorldPosition(int gridX, int gridY) const;

    // 現在の選択カーソル位置を設定する関数
    // 引数 x, y: 格子座標
    void SetCursorPosition(int x, int y)
    {
        m_cursorX = x;
        m_cursorY = y;
    }

    // 盤面の横マス数を取得する関数
    int GetBoardWidth() const
    {
        return m_boardWidth;
    }

    // 盤面の縦マス数を取得する関数
    int GetBoardHeight() const
    {
        return m_boardHeight;
    }

    // 盤面サイズを変更リサイズする関数
    // 引数 newWidth: 新しい横マス数, newHeight: 新しい縦マス数
    void ResizeBoard(int newWidth, int newHeight)
    {
        m_boardWidth = newWidth;
        m_boardHeight = newHeight;
        if (m_device && m_context)
        {
            Initialize(m_device, m_context);
        }
    }

    // 指定位置のコマを撃破削除する関数
    // 引数 x, y: 削除する格子座標
    void RemovePieceAt(int x, int y);

    // 指定位置へ新規コマを生成配置する関数
    // 引数 x, y: 配置格子座標, owner: 所有者, type: コマ種別, topValue: 上面出目
    void AddPieceAt(int x, int y, PlayerType owner, PieceType type, int topValue);

private:
    // 戦闘（衝突）時の勝敗判定と生存処理を行う内部関数
    void ResolveBattle(BoardPiece& attacker, BoardPiece& defender, int targetX, int targetY);

    // 対局全滅・終了条件チェックを行う内部関数
    void CheckGameEndConditions();

private:
    ID3D11Device* m_device = nullptr;         // Direct3D11デバイス参照
    ID3D11DeviceContext* m_context = nullptr; // デバイスコンテキスト参照

    int m_boardWidth = DiceBattleConstants::DEFAULT_BOARD_WIDTH;   // 盤面の横マス数
    int m_boardHeight = DiceBattleConstants::DEFAULT_BOARD_HEIGHT; // 盤面の縦マス数

    std::vector<BoardPiece> m_pieces; // シーン上の全コマ配列
    StepDice m_stepDice;              // 歩数決定用ダイス

    PlayerType m_currentTurn = PlayerType::Player1; // 現在のターン所有者
    int m_remainingSteps = 0;                       // 現在のターンで移動可能な残り歩数
    bool m_isWaitingForRoll = false;                // ダイスの転がり演出中フラグ
    bool m_hasRolledThisTurn = false;               // このターンダイスを振ったかフラグ

    float m_rollAnimTimer = 0.0f; // 投入アニメーション経過時間

    int m_targetFixedSteps = 0; // 受信・設定された固定歩数
    int m_justLandedSteps = 0;  // 直前に入目確定した出目

    DirectX::SimpleMath::Vector3 m_throwStartPos;  // 歩数ダイスの投入開始位置
    DirectX::SimpleMath::Vector3 m_throwTargetPos; // 歩数ダイスの投入目標位置

    bool m_isGameOver = false;                 // 対局終了フラグ
    bool m_isDraw = false;                     // 引き分けフラグ
    PlayerType m_winner = PlayerType::Player1; // 勝利プレイヤー

    std::unordered_map<PlayerType, bool> m_isShielded; // 各プレイヤーの守護状態マップ

    bool m_isAdvanceMode = true; // アドバンスモード有効フラグ

    int m_cursorX = 0; // 現在カーソルX座標
    int m_cursorY = 0; // 現在カーソルY座標

    DefeatedCallback m_onPieceDefeated; // 撃破発生時イベントコールバック
};