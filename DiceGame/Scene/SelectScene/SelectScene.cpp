#include "pch.h"
#include "SelectScene.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void SelectScene::OnEnter(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    void const* colorByteCode = nullptr;
    size_t colorByteCodeLength = 0;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);

    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(
        VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, colorByteCode, colorByteCodeLength,
        m_inputLayout.ReleaseAndGetAddressOf());

    m_mouseTracker.Reset();

    // モード選択シーン用のJSON背景配置を読み込み
    m_sceneModels.Initialize(device);
    m_sceneModels.LoadFromJson(device, "Resources/Data/select_scene.json");
}

void SelectScene::UpdateLayout(const RECT& outputSize)
{
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    float btnWidth = 260.0f;
    float btnHeight = 60.0f;

    float leftX = screenWidth * 0.28f;
    float rightX = screenWidth * 0.72f;
    float btnY = screenHeight * 0.78f;

    m_originalBtnRect = {static_cast<LONG>(leftX - btnWidth * 0.5f), static_cast<LONG>(btnY - btnHeight * 0.5f),
                         static_cast<LONG>(leftX + btnWidth * 0.5f), static_cast<LONG>(btnY + btnHeight * 0.5f)};

    m_advanceBtnRect = {static_cast<LONG>(rightX - btnWidth * 0.5f), static_cast<LONG>(btnY - btnHeight * 0.5f),
                        static_cast<LONG>(rightX + btnWidth * 0.5f), static_cast<LONG>(btnY + btnHeight * 0.5f)};
}

void SelectScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    UpdateLayout(outputSize);

    auto mouseState = DirectX::Mouse::Get().GetState();
    m_mouseTracker.Update(mouseState);

    m_isOriginalHovered = (mouseState.x >= m_originalBtnRect.left && mouseState.x <= m_originalBtnRect.right &&
                           mouseState.y >= m_originalBtnRect.top && mouseState.y <= m_originalBtnRect.bottom);

    m_isAdvanceHovered = (mouseState.x >= m_advanceBtnRect.left && mouseState.x <= m_advanceBtnRect.right &&
                          mouseState.y >= m_advanceBtnRect.top && mouseState.y <= m_advanceBtnRect.bottom);

    if (m_mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
    {
        if (m_isOriginalHovered)
        {
            gameContext.selectedMode = GameMode::Original;
            sceneController.RequestSwitch(SceneId::OriginalGamePlayScene);
        }
        else if (m_isAdvanceHovered)
        {
            gameContext.selectedMode = GameMode::Advance;
            sceneController.RequestSwitch(SceneId::AdvanceGamePlayScene);
        }
    }
}

void SelectScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    auto outputSize = gameContext.deviceResources.GetOutputSize();

    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    // ── 1. JSONモデルの3D描画 ──
    Matrix view = Matrix::CreateLookAt(Vector3(0.0f, 10.0f, -20.0f), Vector3::Zero, Vector3::Up);
    Matrix projection =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), screenWidth / screenHeight, 0.1f, 1000.0f);

    m_sceneModels.Render(context, gameContext.commonStates, view, projection);

    // ── 2. 2D UI 描画 ──
    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
    context->RSSetState(gameContext.commonStates.CullNone());

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    context->IASetInputLayout(m_inputLayout.Get());

    m_colorEffect->Apply(context);
    m_colorPrimitiveBatch->Begin();

    auto DrawButton = [&](const RECT& rect, bool isHovered)
    {
        Vector4 col = isHovered ? Vector4(0.2f, 0.6f, 0.9f, 0.9f) : Vector4(0.1f, 0.2f, 0.4f, 0.8f);
        float l = static_cast<float>(rect.left);
        float r = static_cast<float>(rect.right);
        float t = static_cast<float>(rect.top);
        float b = static_cast<float>(rect.bottom);

        VertexPositionColor v0(Vector3(l, t, 0.0f), col);
        VertexPositionColor v1(Vector3(r, t, 0.0f), col);
        VertexPositionColor v2(Vector3(l, b, 0.0f), col);
        VertexPositionColor v3(Vector3(r, b, 0.0f), col);
        m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
        m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
    };

    DrawButton(m_originalBtnRect, m_isOriginalHovered);
    DrawButton(m_advanceBtnRect, m_isAdvanceHovered);

    m_colorPrimitiveBatch->End();

    // ── 3. テキスト描画 ──
    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;

    float titleX = screenWidth * 0.5f - 80.0f;
    float titleY = screenHeight * 0.20f;
    debugRenderer.DrawText({titleX, titleY}, L"SELECT MODE");

    float origCenterX = static_cast<float>(m_originalBtnRect.left + m_originalBtnRect.right) * 0.5f;
    float origCenterY = static_cast<float>(m_originalBtnRect.top + m_originalBtnRect.bottom) * 0.5f;
    debugRenderer.DrawText({origCenterX - 40.0f, origCenterY - 10.0f}, L"ORIGINAL");
    debugRenderer.DrawText({origCenterX - 65.0f, origCenterY + 40.0f}, L"NO SKILL MODE");

    float advCenterX = static_cast<float>(m_advanceBtnRect.left + m_advanceBtnRect.right) * 0.5f;
    float advCenterY = static_cast<float>(m_advanceBtnRect.top + m_advanceBtnRect.bottom) * 0.5f;
    debugRenderer.DrawText({advCenterX - 35.0f, advCenterY - 10.0f}, L"ADVANCE");
    debugRenderer.DrawText({advCenterX - 60.0f, advCenterY + 40.0f}, L"SKILL ENABLED");

    debugRenderer.Render(context, &gameContext.commonStates);
}