#pragma once

#include <memory>
#include <array>
#include <string>
#include <wrl/client.h>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include "../System/DiceBattleGame.h"
#include "Framework/ObjectData.h"
#include "CameraButton.h" // ★ 作成した CameraButton をインクルード

class GamePlayUIController
{
public:
    GamePlayUIController() = default;
    ~GamePlayUIController() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    void SetCameraBtnFromPlacedObject(const PlacedObject& obj, float orthoWidth, float orthoHeight, float screenWidth,
                                      float screenHeight)
    {
        m_cameraButton.SetFromPlacedObject(obj, orthoWidth, orthoHeight, screenWidth, screenHeight);
    }

    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight, bool isClient,
                PlayerType currentTurn, PlayerType myPlayerType, bool isWaitingForRoll, int remainingSteps,
                bool isAnyPieceMoving, bool isDiceViewerOpen, bool isGameOver, bool isLogButtonHovered, bool isSelected,
                RECT& outLogButtonRect);

    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states, ID3D11Device* device,
                float screenWidth, float screenHeight, PlayerType currentTurn, PlayerType myPlayerType,
                bool isWaitingForRoll, int remainingSteps, bool isDiceViewerOpen, bool isGameOver, bool isDraw,
                PlayerType winnerPlayer, const std::wstring& turnNoticeText, float turnNoticeTimer,
                bool isLogButtonHovered, const RECT& logRect);

    // --- 判定状態（ホバー）の取得 ---
    bool IsRollButtonHovered() const
    {
        return m_isRollButtonHovered;
    }
    bool IsDiceCheckBtnHovered() const
    {
        return m_isDiceCheckBtnHovered;
    }
    bool IsDiceCheckResetHovered() const
    {
        return m_isDiceCheckResetHovered;
    }
    bool IsDiceCheckCloseHovered() const
    {
        return m_isDiceCheckCloseHovered;
    }

    // ★ CameraButton に委譲
    bool IsCameraBtnHovered() const
    {
        return m_cameraButton.IsHovered();
    }

    bool IsUpHovered() const
    {
        return m_isUpHovered;
    }
    bool IsDownHovered() const
    {
        return m_isDownHovered;
    }
    bool IsLeftHovered() const
    {
        return m_isLeftHovered;
    }
    bool IsRightHovered() const
    {
        return m_isRightHovered;
    }

    const RECT& GetRollButtonRect() const
    {
        return m_rollButtonRect;
    }
    const RECT& GetDiceCheckRect() const
    {
        return m_btnDiceCheckRect;
    }

    // ★ CameraButton に委譲
    const RECT& GetCameraBtnRect() const
    {
        return m_cameraButton.GetRect();
    }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rollDiceButtonTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_diceCheckButtonTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowUpTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowDownTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowLeftTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowRightTexture;

    CameraButton m_cameraButton; // ★ カメラボタンインスタンス

    RECT m_rollButtonRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckCloseRect = {0, 0, 0, 0};
    RECT m_btnDiceCheckResetRect = {0, 0, 0, 0};

    RECT m_btnUpRect = {0, 0, 0, 0};
    RECT m_btnDownRect = {0, 0, 0, 0};
    RECT m_btnLeftRect = {0, 0, 0, 0};
    RECT m_btnRightRect = {0, 0, 0, 0};

    bool m_isRollButtonHovered = false;
    bool m_isDiceCheckBtnHovered = false;
    bool m_isDiceCheckResetHovered = false;
    bool m_isDiceCheckCloseHovered = false;

    bool m_isUpHovered = false;
    bool m_isDownHovered = false;
    bool m_isLeftHovered = false;
    bool m_isRightHovered = false;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
};