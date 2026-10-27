#include "pch.h"
#include "GamePlayUIController.h"
#include <WICTextureLoader.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void GamePlayUIController::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionTexture>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetVertexColorEnabled(false);

    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    CreateWICTextureFromFile(device, L"Resources/Textures/RollDice.png", nullptr,
                             m_rollDiceButtonTexture.ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/DiceCheck.png", nullptr,
                             m_diceCheckButtonTexture.ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/LOG.png", nullptr,
                             m_logButtonTexture.ReleaseAndGetAddressOf());

    
    m_cameraButton.Initialize(device);

    CreateWICTextureFromFile(device, L"Resources/Textures/Flont.png", nullptr,
                             m_arrowUpTexture.ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/Back.png", nullptr,
                             m_arrowDownTexture.ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/Left.png", nullptr,
                             m_arrowLeftTexture.ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/Right.png", nullptr,
                             m_arrowRightTexture.ReleaseAndGetAddressOf());
}

void GamePlayUIController::Update(const Mouse::State& mouseState, float screenWidth, float screenHeight, bool isClient,
                                  PlayerType currentTurn, PlayerType myPlayerType, bool isWaitingForRoll,
                                  int remainingSteps, bool isAnyPieceMoving, bool isDiceViewerOpen, bool isGameOver,
                                  bool isLogButtonHovered, bool isSelected, RECT& outLogButtonRect)
{
    (void)isClient;
    (void)isWaitingForRoll;
    (void)remainingSteps;
    (void)isAnyPieceMoving;
    (void)isGameOver;
    (void)isLogButtonHovered;
    (void)isSelected;

   
    m_cameraButton.Update(mouseState, screenWidth, screenHeight, isDiceViewerOpen);

    if (isDiceViewerOpen)
    {
        m_btnDiceCheckResetRect = {static_cast<LONG>(screenWidth - 270), 30, static_cast<LONG>(screenWidth - 150), 70};
        m_btnDiceCheckCloseRect = {static_cast<LONG>(screenWidth - 130), 30, static_cast<LONG>(screenWidth - 10), 70};

        m_isDiceCheckCloseHovered =
            (mouseState.x >= m_btnDiceCheckCloseRect.left && mouseState.x <= m_btnDiceCheckCloseRect.right &&
             mouseState.y >= m_btnDiceCheckCloseRect.top && mouseState.y <= m_btnDiceCheckCloseRect.bottom);
        m_isDiceCheckResetHovered =
            (mouseState.x >= m_btnDiceCheckResetRect.left && mouseState.x <= m_btnDiceCheckResetRect.right &&
             mouseState.y >= m_btnDiceCheckResetRect.top && mouseState.y <= m_btnDiceCheckResetRect.bottom);

        m_isDiceCheckBtnHovered = false;
        return;
    }

    float topBtnW = 180.0f;
    float topBtnH = 100.0f;
    float margin = 20.0f;

    m_btnDiceCheckRect = {static_cast<LONG>(margin), static_cast<LONG>(margin), static_cast<LONG>(margin + topBtnW),
                          static_cast<LONG>(margin + topBtnH)};
    m_isDiceCheckBtnHovered = (mouseState.x >= m_btnDiceCheckRect.left && mouseState.x <= m_btnDiceCheckRect.right &&
                               mouseState.y >= m_btnDiceCheckRect.top && mouseState.y <= m_btnDiceCheckRect.bottom);

    outLogButtonRect = {static_cast<LONG>(screenWidth - topBtnW - margin), static_cast<LONG>(margin),
                        static_cast<LONG>(screenWidth - margin), static_cast<LONG>(margin + topBtnH)};

    if (currentTurn == myPlayerType)
    {
        m_isRollButtonHovered = (mouseState.x >= m_rollButtonRect.left && mouseState.x <= m_rollButtonRect.right &&
                                 mouseState.y >= m_rollButtonRect.top && mouseState.y <= m_rollButtonRect.bottom);
        m_isUpHovered = (mouseState.x >= m_btnUpRect.left && mouseState.x <= m_btnUpRect.right &&
                         mouseState.y >= m_btnUpRect.top && mouseState.y <= m_btnUpRect.bottom);
        m_isDownHovered = (mouseState.x >= m_btnDownRect.left && mouseState.x <= m_btnDownRect.right &&
                           mouseState.y >= m_btnDownRect.top && mouseState.y <= m_btnDownRect.bottom);
        m_isLeftHovered = (mouseState.x >= m_btnLeftRect.left && mouseState.x <= m_btnLeftRect.right &&
                           mouseState.y >= m_btnLeftRect.top && mouseState.y <= m_btnLeftRect.bottom);
        m_isRightHovered = (mouseState.x >= m_btnRightRect.left && mouseState.x <= m_btnRightRect.right &&
                            mouseState.y >= m_btnRightRect.top && mouseState.y <= m_btnRightRect.bottom);
    }
}

void GamePlayUIController::Render(ID3D11DeviceContext* context, const CommonStates& states, ID3D11Device* device,
                                  float screenWidth, float screenHeight, PlayerType currentTurn,
                                  PlayerType myPlayerType, bool isWaitingForRoll, int remainingSteps,
                                  bool isDiceViewerOpen, bool isGameOver, bool isDraw, PlayerType winnerPlayer,
                                  const std::wstring& turnNoticeText, float turnNoticeTimer, bool isLogButtonHovered,
                                  const RECT& logRect)
{
    (void)isDraw;
    (void)winnerPlayer;
    (void)turnNoticeText;
    (void)turnNoticeTimer;

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.DepthNone(), 0);
    context->RSSetState(states.CullNone());

    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(Matrix::Identity);
    m_effect->SetProjection(orthoProj);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    void const* shaderByteCode;
    size_t byteCodeLength;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    device->CreateInputLayout(VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount,
                              shaderByteCode, byteCodeLength, inputLayout.GetAddressOf());

    auto DrawTextureButton = [&](ID3D11ShaderResourceView* tex, const RECT& rect, bool isHovered)
    {
        if (!tex)
            return;

        float baseW = static_cast<float>(rect.right - rect.left);
        float baseH = static_cast<float>(rect.bottom - rect.top);
        float scale = isHovered ? 1.05f : 1.0f;

        float bWidth = baseW * scale;
        float bHeight = baseH * scale;

        float btnX = static_cast<float>(rect.left) - (bWidth - baseW) * 0.5f;
        float btnY = static_cast<float>(rect.top) - (bHeight - baseH) * 0.5f;

        context->IASetInputLayout(inputLayout.Get());
        m_effect->SetTexture(tex);
        m_effect->Apply(context);

        m_primitiveBatch->Begin();
        VertexPositionTexture v0(Vector3(btnX, btnY, 0.0f), Vector2(0.0f, 0.0f));
        VertexPositionTexture v1(Vector3(btnX + bWidth, btnY, 0.0f), Vector2(1.0f, 0.0f));
        VertexPositionTexture v2(Vector3(btnX, btnY + bHeight, 0.0f), Vector2(0.0f, 1.0f));
        VertexPositionTexture v3(Vector3(btnX + bWidth, btnY + bHeight, 0.0f), Vector2(1.0f, 1.0f));
        m_primitiveBatch->DrawTriangle(v0, v1, v2);
        m_primitiveBatch->DrawTriangle(v2, v1, v3);
        m_primitiveBatch->End();
    };

    if (!isDiceViewerOpen && !isGameOver)
    {
        DrawTextureButton(m_diceCheckButtonTexture.Get(), m_btnDiceCheckRect, m_isDiceCheckBtnHovered);

        // ★ カメラボタンの描画
        m_cameraButton.Render(context, m_primitiveBatch.get(), m_effect.get(), inputLayout.Get());

        DrawTextureButton(m_logButtonTexture.Get(), logRect, isLogButtonHovered);
    }

    bool canControlPiece = (currentTurn == myPlayerType) && !isWaitingForRoll && (remainingSteps > 0);
    if (canControlPiece && !isDiceViewerOpen)
    {
        float arrowCenterX = screenWidth - 160.0f;
        float arrowCenterY = screenHeight - 180.0f;
        float spacing = 15.0f;

        auto DrawArrowButton = [&](ID3D11ShaderResourceView* tex, bool isHovered, float baseW, float baseH, float cx,
                                   float cy, RECT& outRect)
        {
            float scale = isHovered ? 1.1f : 1.0f;
            float btnW = baseW * scale;
            float btnH = baseH * scale;
            float x = cx - btnW * 0.5f;
            float y = cy - btnH * 0.5f;

            outRect = {static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + btnW),
                       static_cast<LONG>(y + btnH)};

            if (tex)
            {
                context->IASetInputLayout(inputLayout.Get());
                m_effect->SetTexture(tex);
                m_effect->Apply(context);

                m_primitiveBatch->Begin();
                VertexPositionTexture v0(Vector3(x, y, 0.0f), Vector2(0.0f, 0.0f));
                VertexPositionTexture v1(Vector3(x + btnW, y, 0.0f), Vector2(1.0f, 0.0f));
                VertexPositionTexture v2(Vector3(x, y + btnH, 0.0f), Vector2(0.0f, 1.0f));
                VertexPositionTexture v3(Vector3(x + btnW, y + btnH, 0.0f), Vector2(1.0f, 1.0f));
                m_primitiveBatch->DrawTriangle(v0, v1, v2);
                m_primitiveBatch->DrawTriangle(v2, v1, v3);
                m_primitiveBatch->End();
            }
        };

        float buttonSize = 85.0f;
        float offsetDistance = buttonSize * 0.85f + spacing;

        DrawArrowButton(m_arrowUpTexture.Get(), m_isUpHovered, buttonSize, buttonSize, arrowCenterX,
                        arrowCenterY - offsetDistance, m_btnUpRect);
        DrawArrowButton(m_arrowDownTexture.Get(), m_isDownHovered, buttonSize, buttonSize, arrowCenterX,
                        arrowCenterY + offsetDistance, m_btnDownRect);
        DrawArrowButton(m_arrowLeftTexture.Get(), m_isLeftHovered, buttonSize, buttonSize,
                        arrowCenterX - offsetDistance, arrowCenterY, m_btnLeftRect);
        DrawArrowButton(m_arrowRightTexture.Get(), m_isRightHovered, buttonSize, buttonSize,
                        arrowCenterX + offsetDistance, arrowCenterY, m_btnRightRect);
    }

    bool canRoll = (currentTurn == myPlayerType && !isWaitingForRoll && !isGameOver && remainingSteps == 0);
    if (canRoll && m_rollDiceButtonTexture && !isDiceViewerOpen)
    {
        float baseWidth = 220.0f, baseHeight = 120.0f;
        float scale = m_isRollButtonHovered ? 1.08f : 1.0f;
        float bWidth = baseWidth * scale, bHeight = baseHeight * scale;
        float btnX = (screenWidth - baseWidth - 20.0f) - (bWidth - baseWidth) * 0.5f;
        float btnY = (screenHeight - baseHeight - 20.0f) - (bHeight - baseHeight) * 0.5f;

        m_rollButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY), static_cast<LONG>(btnX + bWidth),
                            static_cast<LONG>(btnY + bHeight)};
        DrawTextureButton(m_rollDiceButtonTexture.Get(), m_rollButtonRect, m_isRollButtonHovered);
    }
}