#pragma once

#include <array>
#include <memory>
#include <vector>
#include <string>
#include <cwchar>
#include <cstdarg>
#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <WICTextureLoader.h>
#include <GeometricPrimitive.h>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "DiceBattleGame.h"
#include "DiceBattleAI.h"
#include "GridField.h"
#include "PieceDropAnimator.h"
#include "Framework/ObjectData.h"
#include "Framework/SoundManager.h"

#include "GameLogManager.h"
#include "ParticleManager.h"
#include "MovableTileCalculator.h"
#include "CameraManager.h"
#include "GamePlayUIController.h"
#include "GameJudgeManager.h"
#include "PieceMovementController.h"
#include "TurnManager.h"
#include "DiceViewerController.h"
#include "Dice/KnockbackDice.h"
#include "Dice/Dice.h"

class Dice;

// ゲームプレイ共通定数定義
namespace GamePlayBaseConstants
{
    constexpr float DEFAULT_GRID_CELL_SIZE = 2.0f;         // グリッド1マスのサイズ
    constexpr float DEFAULT_AI_THINK_DELAY = 1.0f;         // AIの思考ディレイ時間（秒）
    constexpr float DEFAULT_NOTICE_DURATION = 1.5f;        // ターン通知の標準表示時間（秒）
    constexpr float GAME_START_NOTICE_DURATION = 1.8f;     // ゲーム開始通知の表示時間（秒）
    constexpr float KNOCKBACK_DURATION = 0.8f;             // 撃破ダイス吹っ飛びアニメーション時間（秒）
    constexpr int DEFEAT_PARTICLE_COUNT = 30;              // 被弾時の発生エフェクト粒子数
    constexpr int LOG_BUTTON_CLICK_PARTICLE_COUNT = 20;    // ログボタンクリック時の粒子数
    constexpr int CAMERA_BUTTON_CLICK_PARTICLE_COUNT = 20; // カメラ切り替え時の粒子数
    constexpr int MOUSE_CLICK_PARTICLE_COUNT = 25;         // マウスクリック時の粒子数
    constexpr int DICE_FACE_COUNT = 6;                     // ダイスの面数
    constexpr size_t LOG_BUFFER_SIZE = 512;                // ログフォーマット用バッファサイズ

    constexpr float FOV_ANGLE_Y = 45.0f; // 画角 (度)
    constexpr float NEAR_PLANE = 0.1f;   // 近平面
    constexpr float FAR_PLANE = 1000.0f; // 遠平面

    constexpr float SELECTION_MARKER_SIZE = 0.8f;     // 選択マーカーの底面サイズ
    constexpr float SELECTION_MARKER_HEIGHT = 1.0f;   // 選択マーカーの高さサイズ
    constexpr int SELECTION_MARKER_TESSELLATION = 12; // 選択マーカーの分割数

    constexpr float NOTICE_TEXT_X_RATIO = 0.5f;    // ターン通知表示位置X割合
    constexpr float NOTICE_TEXT_Y_RATIO = 0.35f;   // ターン通知表示位置Y割合
    constexpr float NOTICE_TEXT_CHAR_WIDTH = 8.0f; // ターン通知テキスト1文字幅
}

// ゲームプレイシーンの共通基底クラス
class GamePlaySceneBase : public Imase::SceneBase<SceneId, GameContext>
{
public:
    GamePlaySceneBase();
    virtual ~GamePlaySceneBase();

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    void TriggerTurnNotice(const std::wstring& text, float duration = GamePlayBaseConstants::DEFAULT_NOTICE_DURATION);
    void ResetMoveState();

    // 振り直しスキル使用時などにロール連打ロックを解除する関数
    void UnlockRollButton()
    {
        m_hasClickedRollThisTurn = false;
    }

    void AddLog(const wchar_t* fmt, ...)
    {
        wchar_t buffer[GamePlayBaseConstants::LOG_BUFFER_SIZE];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        m_logManager.AddLog(buffer);
    }

    void SpawnKnockbackDice(ID3D11Device* device, ID3D11DeviceContext* context,
                            const DirectX::SimpleMath::Vector3& attackerPos,
                            const DirectX::SimpleMath::Vector3& victimPos,
                            const std::string& modelPath = "Resources/Models/Dice.cmo")
    {
        auto kbDice = std::make_unique<KnockbackDice>();
        kbDice->Initialize(device, context, modelPath);
        kbDice->TriggerKnockback(attackerPos, victimPos, GamePlayBaseConstants::KNOCKBACK_DURATION);
        m_knockbackDiceList.push_back(std::move(kbDice));
    }

protected:
    virtual void OnModeEnter(GameContext& gameContext) = 0;
    virtual void OnModeUpdate(float dt, GameContext& gameContext) = 0;
    virtual void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                GameContext& gameContext) = 0;
    virtual bool IsModeSelectingTarget() const = 0;
    virtual void OnModeGridClicked(int gridX, int gridY) = 0;

    void SendHostWorldSync(GameContext& gameContext);
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
    Dice* GetTargetDiceInstance();
    std::vector<MovableTileInfo> GetMovableTiles() const;

private:

    void RenderFirstTurnSpotlight(ID3D11DeviceContext* context, const DirectX::CommonStates& states,
                                  const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj);

    DirectX::SimpleMath::Vector3 m_p1FirstStartPos;
    DirectX::SimpleMath::Vector3 m_p1FirstTargetPos;
    DirectX::SimpleMath::Vector3 m_p2FirstStartPos;
    DirectX::SimpleMath::Vector3 m_p2FirstTargetPos;
    float m_firstRollTotalDuration = 1.2f;
    float m_spotlightAlpha = 0.0f;
   
    void StartFirstTurnRoll(GameContext& gameContext);
    void UpdateFirstTurnPhase(float dt, GameContext& gameContext);
    static DirectX::SimpleMath::Quaternion GetDiceFaceRotation(int face);

protected:
    GridField m_field;
    DiceBattleGame m_gameLogic;

    std::vector<std::unique_ptr<KnockbackDice>> m_knockbackDiceList;

    DiceBattleAI m_ai;
    PieceDropAnimator m_dropAnimator;
    ParticleManager m_particleManager;
    GameLogManager m_logManager;
    CameraManager m_cameraManager;

    GamePlayUIController m_uiController;
    std::unique_ptr<GameJudgeManager> m_judgeManager;

    PieceMovementController m_movementController;
    TurnManager m_turnManager;
    DiceViewerController m_diceViewerController;

    std::vector<PlacedObject> m_placedObjects;
    std::vector<PlacedObject> m_loadedObjects;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 7> m_diceCountTextures;

    std::unique_ptr<DirectX::GeometricPrimitive> m_selectionMarker;
    float m_markerAnimTimer = 0.0f;

    bool m_hasClickedRollThisTurn = false; // 連打による二重ロール防止ロック

    // --- 先攻・後攻決定ダイスロール演出用メンバ ---
    enum class FirstTurnPhase
    {
        Rolling,       // 両プレイヤーのダイス回転演出中
        WaitingServer, // 相手のダイス結果とサーバー判定待ち
        ShowingResult, // 出目結果と先攻発表表示中
        Done           // 決定完了（通常対戦中）
    };

    FirstTurnPhase m_firstTurnPhase = FirstTurnPhase::Rolling;
    Dice m_p1FirstDice; // 1P（青）の先攻決め演出用ダイス
    Dice m_p2FirstDice; // 2P（赤）の先攻決め演出用ダイス
    float m_firstRollTimer = 0.0f;
    int m_myFirstRollValue = 0;
    int m_p1ResultRoll = 0;
    int m_p2ResultRoll = 0;
    uint8_t m_decidedFirstPlayer = 0;
    bool m_hasReceivedFirstResult = false;
};