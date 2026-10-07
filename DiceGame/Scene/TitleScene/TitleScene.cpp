#include "pch.h"
#include "TitleScene.h"
#include <Mouse.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void TitleScene::OnEnter(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    // タイトルシーン用のJSON配置を読み込み
    m_sceneModels.Initialize(device);
    m_sceneModels.LoadFromJson(device, "Resources/Data/title_scene.json");
}

// 3D空間のAABB（最小点・最大点）を画面の2D RECTに変換するヘルパー関数
RECT ProjectAABBToScreen(const Vector3& minBounds, const Vector3& maxBounds, const Matrix& view, const Matrix& proj,
                         float screenWidth, float screenHeight)
{
    Vector3 corners[8] = {
        Vector3(minBounds.x, minBounds.y, minBounds.z), Vector3(maxBounds.x, minBounds.y, minBounds.z),
        Vector3(minBounds.x, maxBounds.y, minBounds.z), Vector3(maxBounds.x, maxBounds.y, minBounds.z),
        Vector3(minBounds.x, minBounds.y, maxBounds.z), Vector3(maxBounds.x, minBounds.y, maxBounds.z),
        Vector3(minBounds.x, maxBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
    };

    float minX = screenWidth, minY = screenHeight;
    float maxX = 0.0f, maxY = 0.0f;

    Matrix viewProj = view * proj;

    for (int i = 0; i < 8; ++i)
    {
        Vector4 clip = Vector4::Transform(Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f), viewProj);
        if (clip.w != 0.0f)
        {
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;

            float screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
            float screenY = (1.0f - ndcY) * 0.5f * screenHeight;

            minX = (std::min)(minX, screenX);
            maxX = (std::max)(maxX, screenX);
            minY = (std::min)(minY, screenY);
            maxY = (std::max)(maxY, screenY);
        }
    }

    return RECT{static_cast<LONG>(minX), static_cast<LONG>(minY), static_cast<LONG>(maxX), static_cast<LONG>(maxY)};
}

void TitleScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    // カメラ行列（Renderと同じ画角で計算）
    Matrix view = Matrix::CreateLookAt(Vector3(0.0f, 10.0f, -20.0f), Vector3::Zero, Vector3::Up);
    Matrix projection =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), screenWidth / screenHeight, 0.1f, 1000.0f);

    // ★ エディターで設定した Tag ("StartButton" / "ExitButton") のモデルからボタン矩形を自動生成
    Vector3 minB, maxB;
    if (m_sceneModels.GetObjectAABB("StartButton", minB, maxB))
    {
        m_startBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
    }
    else
    {
        // Tagが設定されていない場合のフォールバック（デフォルト位置）
        float centerX = screenWidth * 0.5f, startY = screenHeight * 0.60f;
        m_startBtnRect = {static_cast<LONG>(centerX - 100.0f), static_cast<LONG>(startY - 25.0f),
                          static_cast<LONG>(centerX + 100.0f), static_cast<LONG>(startY + 25.0f)};
    }

    if (m_sceneModels.GetObjectAABB("ExitButton", minB, maxB))
    {
        m_exitBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
    }
    else
    {
        float centerX = screenWidth * 0.5f, exitY = screenHeight * 0.72f;
        m_exitBtnRect = {static_cast<LONG>(centerX - 100.0f), static_cast<LONG>(exitY - 25.0f),
                         static_cast<LONG>(centerX + 100.0f), static_cast<LONG>(exitY + 25.0f)};
    }

    // マウス入力処理
    auto mouseState = DirectX::Mouse::Get().GetState();
    static DirectX::Mouse::ButtonStateTracker mouseTracker;
    mouseTracker.Update(mouseState);

    m_isStartHovered = (mouseState.x >= m_startBtnRect.left && mouseState.x <= m_startBtnRect.right &&
                        mouseState.y >= m_startBtnRect.top && mouseState.y <= m_startBtnRect.bottom);

    m_isExitHovered = (mouseState.x >= m_exitBtnRect.left && mouseState.x <= m_exitBtnRect.right &&
                       mouseState.y >= m_exitBtnRect.top && mouseState.y <= m_exitBtnRect.bottom);

    if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
    {
        if (m_isStartHovered)
        {
            sceneController.RequestSwitch(SceneId::SelectScene);
        }
        else if (m_isExitHovered)
        {
            PostQuitMessage(0);
        }
    }
}

void TitleScene::Render(GameContext& gameContext)
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

    void const* colorByteCode;
    size_t colorByteCodeLength;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);

    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(VertexPositionColor::InputElements,
                                                                  VertexPositionColor::InputElementCount, colorByteCode,
                                                                  colorByteCodeLength, colorInputLayout.GetAddressOf());
    context->IASetInputLayout(colorInputLayout.Get());

    m_colorEffect->Apply(context);
    m_colorPrimitiveBatch->Begin();

    // ボタン描画関数
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

    DrawButton(m_startBtnRect, m_isStartHovered);
    DrawButton(m_exitBtnRect, m_isExitHovered);

    m_colorPrimitiveBatch->End();

    // ── 3. テキスト描画 ──
    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;

    float mainTextX = screenWidth * 0.5f - 80.0f;
    float mainTextY = screenHeight * 0.35f;
    debugRenderer.DrawText({mainTextX, mainTextY}, L"DICE BATTLE GAME");

    float startTextX = static_cast<float>(m_startBtnRect.left + m_startBtnRect.right) * 0.5f - 25.0f;
    float startTextY = static_cast<float>(m_startBtnRect.top + m_startBtnRect.bottom) * 0.5f - 8.0f;
    debugRenderer.DrawText({startTextX, startTextY}, L"START");

    float exitTextX = static_cast<float>(m_exitBtnRect.left + m_exitBtnRect.right) * 0.5f - 20.0f;
    float exitTextY = static_cast<float>(m_exitBtnRect.top + m_exitBtnRect.bottom) * 0.5f - 8.0f;
    debugRenderer.DrawText({exitTextX, exitTextY}, L"EXIT");

    debugRenderer.Render(context, &gameContext.commonStates);
}