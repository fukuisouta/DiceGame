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

#include "GameLogManager.h"
#include "ParticleManager.h"
#include "DiceViewer.h"

class Dice;

class GamePlaySceneBase : public Imase::SceneBase<SceneId, GameContext>
{
public:
    GamePlaySceneBase() = default;
    virtual ~GamePlaySceneBase() = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    void TriggerTurnNotice(const std::wstring& text, float duration = 1.5f);
    void ResetMoveState();

    // ★ C2039/C7595 回避: vswprintf_s を使用した安全な可変長ログ関数
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
    virtual void OnModeEnter(GameContext& gameContext) = 0;
    virtual void OnModeUpdate(float dt, GameContext& gameContext) = 0;
    virtual void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                GameContext& gameContext) = 0;
    virtual bool IsModeSelectingTarget() const = 0;
    virtual void OnModeGridClicked(int gridX, int gridY) = 0;

    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

    void ExecuteMove(int fromX, int fromY, int toX, int toY, bool isNetworkReceived = false,
                     GameContext* gameContext = nullptr);

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

    DirectX::SimpleMath::Vector3 m_cameraEye = DirectX::SimpleMath::Vector3(0.0f, 22.0f, -20.0f);
    DirectX::SimpleMath::Vector3 m_cameraTarget = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
    float m_cameraPitch = 48.0f;
    float m_cameraDistance = 30.0f;

    bool m_isGameOver = false;
    bool m_isDraw = false;
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

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowUpTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowDownTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowLeftTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowRightTexture;

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

    DiceViewer m_diceViewer;
    RECT m_btnDiceCheckRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckCloseRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckResetRect = {0, 0, 0, 0};
    bool m_isDiceCheckBtnHovered = false;
    bool m_isDiceCheckCloseHovered = false;
    bool m_isDiceCheckResetHovered = false;

    std::unique_ptr<DirectX::GeometricPrimitive> m_selectionMarker;
    float m_markerAnimTimer = 0.0f;
};