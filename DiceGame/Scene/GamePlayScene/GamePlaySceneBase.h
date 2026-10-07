#pragma once

#include <array>
#include <memory>
#include <vector>
#include <string>
#include <format>
#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <WICTextureLoader.h>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "DiceBattleGame.h"
#include "DiceBattleAI.h"
#include "GridField.h"
#include "PieceDropAnimator.h"
#include "Framework/ObjectData.h"

#include "GameLogManager.h"
#include "ParticleManager.h"
#include "DiceViewer.h"

// 前方宣言
class Dice;

class GamePlaySceneBase : public Imase::SceneBase<SceneId, GameContext>
{
public:
    GamePlaySceneBase() = default;
    virtual ~GamePlaySceneBase() = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    // --- ヘルパーメソッド群 ---
    void TriggerTurnNotice(const std::wstring& text, float duration = 1.5f);
    void ResetMoveState();

    template <typename... Args> void AddLog(const wchar_t* fmt, Args&&... args)
    {
        if constexpr (sizeof...(Args) == 0)
            m_logManager.AddLog(fmt);
        else
            m_logManager.AddLog(std::vformat(fmt, std::make_wformat_args(args...)));
    }

protected:
    // モード固有の拡張用ピュア仮想関数
    virtual void OnModeEnter(GameContext& gameContext) = 0;
    virtual void OnModeUpdate(float dt, GameContext& gameContext) = 0;
    virtual void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                GameContext& gameContext) = 0;
    virtual bool IsModeSelectingTarget() const = 0;
    virtual void OnModeGridClicked(int gridX, int gridY) = 0;

    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
    void ExecuteMove(int fromX, int fromY, int toX, int toY);

    // プレビュー対象の Dice インスタンスを取得する関数
    Dice* GetTargetDiceInstance();

protected:
    GridField m_field;
    DiceBattleGame m_gameLogic;
    DiceBattleAI m_ai;
    PieceDropAnimator m_dropAnimator;
    ParticleManager m_particleManager;
    GameLogManager m_logManager;

    std::vector<PlacedObject> m_placedObjects;
    std::vector<PlacedObject> m_loadedObjects;

    int m_cursorX = -1;
    int m_cursorY = -1;
    int m_selectedX = -1;
    int m_selectedY = -1;

    // プレイ画面のカメラ位置（手前から奥を見るように Z を -20.0f に設定）
    DirectX::SimpleMath::Vector3 m_cameraEye = DirectX::SimpleMath::Vector3(0.0f, 22.0f, -20.0f);
    DirectX::SimpleMath::Vector3 m_cameraTarget = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
    float m_cameraPitch = 48.0f;
    float m_cameraDistance = 30.0f;

    bool m_isGameOver = false;
    PlayerType m_winnerPlayer = PlayerType::Player1;
    int m_gameOverSelectIndex = 0;
    RECT m_retryButtonRect = {0, 0, 0, 0};
    RECT m_titleButtonRect = {0, 0, 0, 0};
    bool m_isRetryHovered = false;
    bool m_isTitleHovered = false;

    PlayerType m_previousTurn = PlayerType::Player1;
    std::wstring m_turnNoticeText = L"";
    float m_turnNoticeTimer = 0.0f;
    float m_turnNoticeMaxTime = 1.5f;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rollDiceButtonTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_diceCheckButtonTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture;

    // ★ 矢印ボタン用テクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowUpTexture;    // Flont.png (上方向)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowDownTexture;  // Buck.png (下方向)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowLeftTexture;  // Left.png
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowRightTexture; // Right.png

    RECT m_rollButtonRect = {0, 0, 0, 0};
    bool m_isRollButtonHovered = false;

    RECT m_btnUpRect = {0, 0, 0, 0};
    RECT m_btnDownRect = {0, 0, 0, 0};
    RECT m_btnLeftRect = {0, 0, 0, 0};
    RECT m_btnRightRect = {0, 0, 0, 0};
    bool m_isUpHovered = false;
    bool m_isDownHovered = false;
    bool m_isLeftHovered = false;
    bool m_isRightHovered = false;

    bool m_hasMovedThisTurn = false;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 4> m_diceCountTextures;

    // --- ダイスチェック用メンバ ---
    DiceViewer m_diceViewer;
    RECT m_btnDiceCheckRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckCloseRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckResetRect = {0, 0, 0, 0};
    bool m_isDiceCheckBtnHovered = false;
    bool m_isDiceCheckCloseHovered = false;
    bool m_isDiceCheckResetHovered = false;
};