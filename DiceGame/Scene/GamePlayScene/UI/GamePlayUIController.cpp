#include "pch.h"
#include "GamePlayUIController.h"
#include <WICTextureLoader.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// UI操作・表示スタイル定数定義
namespace UIControllerStyleConstants
{
    constexpr float MOVE_SPEED_NORMAL = 2.0f;          // デバッグUI調整の通常移動スピード
    constexpr float MOVE_SPEED_FAST = 10.0f;           // デバッグUI調整の高速移動スピード (Shift併用)
    constexpr float MIN_BUTTON_SIZE = 10.0f;           // デバッグ調整での最小ボタン幅・高さ
    constexpr float BUTTON_SCALE_HOVER_NORMAL = 1.05f; // 通常ボタンホバー拡大倍率
    constexpr float BUTTON_SCALE_HOVER_ROLL = 1.08f;   // ダイス振りボタンホバー拡大倍率
    constexpr float ARROW_SCALE_HOVER = 1.1f;          // 矢印ボタンホバー拡大倍率
    constexpr float ARROW_CENTER_X_OFFSET = 160.0f;    // 矢印群中心Xオフセット
    constexpr float ARROW_CENTER_Y_OFFSET = 180.0f;    // 矢印群中心Yオフセット
    constexpr float ARROW_SPACING_FACTOR = 0.85f;      // 矢印配置オフセット係数
    constexpr float RELATIVE_CORNER_THRESHOLD = 0.99f; // 右端・下端判定の相対比率閾値

    constexpr LONG VIEWER_BTN_Y_TOP = 30;           // ビューワー用ボタン上端Y
    constexpr LONG VIEWER_BTN_Y_BOTTOM = 70;        // ビューワー用ボタン下端Y
    constexpr LONG VIEWER_RESET_OFFSET_RIGHT = 270; // ビューワーリセットボタン右位置オフセット
    constexpr LONG VIEWER_RESET_WIDTH = 120;        // ビューワーリセットボタン幅
    constexpr LONG VIEWER_CLOSE_OFFSET_RIGHT = 130; // ビューワー閉じるボタン右位置オフセット
    constexpr LONG VIEWER_CLOSE_WIDTH = 120;        // ビューワー閉じるボタン幅
}

// 初期化
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

    LoadLayoutFromJson("Resources/Data/ui_layout.json");
}

// リアルタイムUI調整＆セーブ入力処理
void GamePlayUIController::UpdateDebugAdjustment(const DirectX::Keyboard::KeyboardStateTracker& kbTracker,
                                                 const DirectX::Keyboard::State& kbState)
{
    if (kbTracker.IsKeyPressed(DirectX::Keyboard::Keys::F5))
    {
        if (SaveLayoutToJson())
        {
            OutputDebugStringA("[UIController] UI Layout successfully saved to Resources/Data/ui_layout.json\n");
        }
    }

    if (kbTracker.IsKeyPressed(DirectX::Keyboard::Keys::F1))
    {
        if (m_debugTarget == UIDebugTarget::None)
            m_debugTarget = UIDebugTarget::DiceCheck;
        else if (m_debugTarget == UIDebugTarget::DiceCheck)
            m_debugTarget = UIDebugTarget::Camera;
        else if (m_debugTarget == UIDebugTarget::Camera)
            m_debugTarget = UIDebugTarget::Log;
        else if (m_debugTarget == UIDebugTarget::Log)
            m_debugTarget = UIDebugTarget::RollDice;
        else
            m_debugTarget = UIDebugTarget::None;
    }

    if (m_debugTarget == UIDebugTarget::None)
        return;

    UIButtonLayout* targetLayout = nullptr;
    if (m_debugTarget == UIDebugTarget::DiceCheck)
        targetLayout = &m_diceCheckLayout;
    else if (m_debugTarget == UIDebugTarget::Camera)
        targetLayout = &m_cameraLayout;
    else if (m_debugTarget == UIDebugTarget::Log)
        targetLayout = &m_logLayout;
    else if (m_debugTarget == UIDebugTarget::RollDice)
        targetLayout = &m_rollLayout;

    if (!targetLayout)
        return;

    float moveSpeed =
        kbState.LeftShift ? UIControllerStyleConstants::MOVE_SPEED_FAST : UIControllerStyleConstants::MOVE_SPEED_NORMAL;

    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Left))
        targetLayout->marginX -= moveSpeed;
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Right))
        targetLayout->marginX += moveSpeed;
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Up))
        targetLayout->marginY -= moveSpeed;
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Down))
        targetLayout->marginY += moveSpeed;

    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::PageUp))
    {
        targetLayout->width += moveSpeed;
        targetLayout->height += moveSpeed * 0.5f;
    }
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::PageDown))
    {
        targetLayout->width = (std::max)(UIControllerStyleConstants::MIN_BUTTON_SIZE, targetLayout->width - moveSpeed);
        targetLayout->height =
            (std::max)(UIControllerStyleConstants::MIN_BUTTON_SIZE, targetLayout->height - moveSpeed * 0.5f);
    }
}

// マウスホバーおよびレイアウト位置の更新
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

    float camX = screenWidth * m_cameraLayout.relX + m_cameraLayout.marginX;
    float camY = screenHeight * m_cameraLayout.relY + m_cameraLayout.marginY;
    RECT camRect = {static_cast<LONG>(camX), static_cast<LONG>(camY), static_cast<LONG>(camX + m_cameraLayout.width),
                    static_cast<LONG>(camY + m_cameraLayout.height)};

    m_cameraButton.SetRect(camRect);
    m_cameraButton.Update(mouseState, screenWidth, screenHeight, isDiceViewerOpen);

    if (isDiceViewerOpen)
    {
        LONG resetLeft = static_cast<LONG>(screenWidth - UIControllerStyleConstants::VIEWER_RESET_OFFSET_RIGHT);
        LONG resetRight = static_cast<LONG>(resetLeft + UIControllerStyleConstants::VIEWER_RESET_WIDTH);
        m_btnDiceCheckResetRect = {resetLeft, UIControllerStyleConstants::VIEWER_BTN_Y_TOP, resetRight,
                                   UIControllerStyleConstants::VIEWER_BTN_Y_BOTTOM};

        LONG closeLeft = static_cast<LONG>(screenWidth - UIControllerStyleConstants::VIEWER_CLOSE_OFFSET_RIGHT);
        LONG closeRight = static_cast<LONG>(closeLeft + UIControllerStyleConstants::VIEWER_CLOSE_WIDTH);
        m_btnDiceCheckCloseRect = {closeLeft, UIControllerStyleConstants::VIEWER_BTN_Y_TOP, closeRight,
                                   UIControllerStyleConstants::VIEWER_BTN_Y_BOTTOM};

        m_isDiceCheckCloseHovered =
            (mouseState.x >= m_btnDiceCheckCloseRect.left && mouseState.x <= m_btnDiceCheckCloseRect.right &&
             mouseState.y >= m_btnDiceCheckCloseRect.top && mouseState.y <= m_btnDiceCheckCloseRect.bottom);
        m_isDiceCheckResetHovered =
            (mouseState.x >= m_btnDiceCheckResetRect.left && mouseState.x <= m_btnDiceCheckResetRect.right &&
             mouseState.y >= m_btnDiceCheckResetRect.top && mouseState.y <= m_btnDiceCheckResetRect.bottom);

        m_isDiceCheckBtnHovered = false;
        return;
    }

    float dcX = screenWidth * m_diceCheckLayout.relX + m_diceCheckLayout.marginX;
    float dcY = screenHeight * m_diceCheckLayout.relY + m_diceCheckLayout.marginY;
    m_btnDiceCheckRect = {static_cast<LONG>(dcX), static_cast<LONG>(dcY),
                          static_cast<LONG>(dcX + m_diceCheckLayout.width),
                          static_cast<LONG>(dcY + m_diceCheckLayout.height)};

    m_isDiceCheckBtnHovered = (mouseState.x >= m_btnDiceCheckRect.left && mouseState.x <= m_btnDiceCheckRect.right &&
                               mouseState.y >= m_btnDiceCheckRect.top && mouseState.y <= m_btnDiceCheckRect.bottom);

    float logX = screenWidth * m_logLayout.relX + m_logLayout.marginX;
    if (m_logLayout.relX >= UIControllerStyleConstants::RELATIVE_CORNER_THRESHOLD)
        logX -= m_logLayout.width;
    float logY = screenHeight * m_logLayout.relY + m_logLayout.marginY;

    outLogButtonRect = {static_cast<LONG>(logX), static_cast<LONG>(logY), static_cast<LONG>(logX + m_logLayout.width),
                        static_cast<LONG>(logY + m_logLayout.height)};

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

// 描画処理
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

    auto DrawTextureButton = [&](ID3D11ShaderResourceView* tex, const RECT& rect, bool isHovered,
                                 float hoverScale = UIControllerStyleConstants::BUTTON_SCALE_HOVER_NORMAL)
    {
        if (!tex)
            return;

        float baseW = static_cast<float>(rect.right - rect.left);
        float baseH = static_cast<float>(rect.bottom - rect.top);
        float scale = isHovered ? hoverScale : 1.0f;

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

    auto DrawDebugHighlight = [&](const RECT& rect)
    {
        void const* colorByteCode;
        size_t colorByteCodeLength;
        m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
        Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
        device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                  colorByteCode, colorByteCodeLength, colorInputLayout.GetAddressOf());

        context->IASetInputLayout(colorInputLayout.Get());
        m_colorEffect->Apply(context);

        m_colorPrimitiveBatch->Begin();
        Vector4 col(1.0f, 1.0f, 0.0f, 0.8f);
        float l = static_cast<float>(rect.left);
        float t = static_cast<float>(rect.top);
        float r = static_cast<float>(rect.right);
        float b = static_cast<float>(rect.bottom);

        VertexPositionColor p0(Vector3(l, t, 0.0f), col);
        VertexPositionColor p1(Vector3(r, t, 0.0f), col);
        VertexPositionColor p2(Vector3(r, b, 0.0f), col);
        VertexPositionColor p3(Vector3(l, b, 0.0f), col);

        m_colorPrimitiveBatch->DrawLine(p0, p1);
        m_colorPrimitiveBatch->DrawLine(p1, p2);
        m_colorPrimitiveBatch->DrawLine(p2, p3);
        m_colorPrimitiveBatch->DrawLine(p3, p0);
        m_colorPrimitiveBatch->End();
    };

    if (!isDiceViewerOpen && !isGameOver)
    {
        DrawTextureButton(m_diceCheckButtonTexture.Get(), m_btnDiceCheckRect, m_isDiceCheckBtnHovered);
        m_cameraButton.Render(context, m_primitiveBatch.get(), m_effect.get(), inputLayout.Get());
        DrawTextureButton(m_logButtonTexture.Get(), logRect, isLogButtonHovered);
    }

    bool canControlPiece = (currentTurn == myPlayerType) && !isWaitingForRoll && (remainingSteps > 0);
    if (canControlPiece && !isDiceViewerOpen)
    {
        float arrowCenterX = screenWidth * m_arrowRelX - UIControllerStyleConstants::ARROW_CENTER_X_OFFSET;
        float arrowCenterY = screenHeight * m_arrowRelY - UIControllerStyleConstants::ARROW_CENTER_Y_OFFSET;

        auto DrawArrowButton = [&](ID3D11ShaderResourceView* tex, bool isHovered, float baseW, float baseH, float cx,
                                   float cy, RECT& outRect)
        {
            float scale = isHovered ? UIControllerStyleConstants::ARROW_SCALE_HOVER : 1.0f;
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

        float buttonSize = m_arrowButtonSize;
        float offsetDistance = buttonSize * UIControllerStyleConstants::ARROW_SPACING_FACTOR + m_arrowSpacing;

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
        float baseWidth = m_rollLayout.width;
        float baseHeight = m_rollLayout.height;

        float btnX = screenWidth * m_rollLayout.relX + m_rollLayout.marginX;
        if (m_rollLayout.relX >= UIControllerStyleConstants::RELATIVE_CORNER_THRESHOLD)
            btnX -= baseWidth;
        float btnY = screenHeight * m_rollLayout.relY + m_rollLayout.marginY;
        if (m_rollLayout.relY >= UIControllerStyleConstants::RELATIVE_CORNER_THRESHOLD)
            btnY -= baseHeight;

        m_rollButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY), static_cast<LONG>(btnX + baseWidth),
                            static_cast<LONG>(btnY + baseHeight)};
        DrawTextureButton(m_rollDiceButtonTexture.Get(), m_rollButtonRect, m_isRollButtonHovered,
                          UIControllerStyleConstants::BUTTON_SCALE_HOVER_ROLL);
    }

    if (m_debugTarget == UIDebugTarget::DiceCheck)
        DrawDebugHighlight(m_btnDiceCheckRect);
    else if (m_debugTarget == UIDebugTarget::Camera)
        DrawDebugHighlight(m_cameraButton.GetRect());
    else if (m_debugTarget == UIDebugTarget::Log)
        DrawDebugHighlight(logRect);
    else if (m_debugTarget == UIDebugTarget::RollDice)
        DrawDebugHighlight(m_rollButtonRect);
}