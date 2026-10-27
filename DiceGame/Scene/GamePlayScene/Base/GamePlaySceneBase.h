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

class Dice;

// ============================================================================
// ゲームプレイシーンの共通基底クラス（画面全体のシステム・UI・レンダリングを集中管理）
// ============================================================================
class GamePlaySceneBase : public Imase::SceneBase<SceneId, GameContext>
{
public:
    GamePlaySceneBase();
    virtual ~GamePlaySceneBase();

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    // 画面中央のターン・イベント通知テキスト表示
    void TriggerTurnNotice(const std::wstring& text, float duration = 1.5f);
    // コマ選択・移動状態のリセット
    void ResetMoveState();

    // ログメッセージのフォーマット付き追加
    void AddLog(const wchar_t* fmt, ...)
    {
        wchar_t buffer[512];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        m_logManager.AddLog(buffer);
    }

protected:
    // モード別の抽象メソッド
    virtual void OnModeEnter(GameContext& gameContext) = 0;
    virtual void OnModeUpdate(float dt, GameContext& gameContext) = 0;
    virtual void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                GameContext& gameContext) = 0;
    virtual bool IsModeSelectingTarget() const = 0;
    virtual void OnModeGridClicked(int gridX, int gridY) = 0;

    // JSONによるステージ配置データロード
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

    // 現在操作・注目中のダイスインスタンスを取得
    Dice* GetTargetDiceInstance();

    // 選択中コマの移動可能マス情報を算出
    std::vector<MovableTileInfo> GetMovableTiles() const;

protected:
    GridField m_field;                 // グリッド盤面表示
    DiceBattleGame m_gameLogic;        // ダイスバトル対戦ロジック
    DiceBattleAI m_ai;                 // AI思考システム
    PieceDropAnimator m_dropAnimator;  // ゲーム開始時のコマ落下演出
    ParticleManager m_particleManager; // クリック等のエフェクト粒子
    GameLogManager m_logManager;       // 行動ログ表示ウィンドウ
    CameraManager m_cameraManager;     // 視点カメラ制御

    GamePlayUIController m_uiController; // ゲーム中UI制御

    // 安全な破棄順序を保障するための unique_ptr
    std::unique_ptr<GameJudgeManager> m_judgeManager;

    PieceMovementController m_movementController; // コマ移動・選択ロジック
    TurnManager m_turnManager;                    // ターン切り替え通知
    DiceViewerController m_diceViewerController;  // ダイス詳細ビュー画面

    std::vector<PlacedObject> m_placedObjects; // シーンデータ
    std::vector<PlacedObject> m_loadedObjects; // ロードされた背景オブジェクト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 7> m_diceCountTextures;

    std::unique_ptr<DirectX::GeometricPrimitive> m_selectionMarker; // マーカー描画用
    float m_markerAnimTimer = 0.0f;
};