#include "pch.h"
#include "CameraButton.h"
#include <WICTextureLoader.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void CameraButton::Initialize(ID3D11Device* device)
{
    // テクスチャ（CAMERA.jpg / CAMERA.png）のロード
    HRESULT hr =
        CreateWICTextureFromFile(device, L"Resources/Textures/CAMERA.jpg", nullptr, m_texture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        CreateWICTextureFromFile(device, L"Resources/Textures/CAMERA.png", nullptr, m_texture.ReleaseAndGetAddressOf());
    }
}

void CameraButton::SetFromPlacedObject(const PlacedObject& obj, float orthoWidth, float orthoHeight, float screenWidth,
                                       float screenHeight)
{
    float leftW = obj.position.x - (obj.scale.x * 0.5f);
    float rightW = obj.position.x + (obj.scale.x * 0.5f);
    float topW = obj.position.y + (obj.scale.y * 0.5f);
    float bottomW = obj.position.y - (obj.scale.y * 0.5f);

    float x1 = (0.5f - leftW / orthoWidth) * screenWidth;
    float x2 = (0.5f - rightW / orthoWidth) * screenWidth;
    float y1 = (0.5f - topW / orthoHeight) * screenHeight;
    float y2 = (0.5f - bottomW / orthoHeight) * screenHeight;

    m_rect.left = static_cast<LONG>((std::min)(x1, x2));
    m_rect.right = static_cast<LONG>((std::max)(x1, x2));
    m_rect.top = static_cast<LONG>((std::min)(y1, y2));
    m_rect.bottom = static_cast<LONG>((std::max)(y1, y2));
}

void CameraButton::Update(const Mouse::State& mouseState, float screenWidth, float screenHeight, bool isDiceViewerOpen)
{
    if (isDiceViewerOpen)
    {
        m_isHovered = false;
        return;
    }

    // デフォルト領域の補完（配置未読み込み時の予備座標）
    if (m_rect.left == 0 && m_rect.right == 0)
    {
        float margin = 20.0f;
        float topBtnW = 180.0f;
        float camX = margin + topBtnW + 15.0f;
        m_rect = {static_cast<LONG>(camX), static_cast<LONG>(margin), static_cast<LONG>(camX + 130.0f),
                  static_cast<LONG>(margin + 60.0f)};
    }

    m_isHovered = (mouseState.x >= m_rect.left && mouseState.x <= m_rect.right && mouseState.y >= m_rect.top &&
                   mouseState.y <= m_rect.bottom);
}

void CameraButton::Render(ID3D11DeviceContext* context, PrimitiveBatch<VertexPositionTexture>* primitiveBatch,
                          BasicEffect* effect, ID3D11InputLayout* inputLayout)
{
    if (!m_texture || !primitiveBatch || !effect || !inputLayout)
        return;

    float baseW = static_cast<float>(m_rect.right - m_rect.left);
    float baseH = static_cast<float>(m_rect.bottom - m_rect.top);
    float scale = m_isHovered ? 1.05f : 1.0f;

    float bWidth = baseW * scale;
    float bHeight = baseH * scale;

    float btnX = static_cast<float>(m_rect.left) - (bWidth - baseW) * 0.5f;
    float btnY = static_cast<float>(m_rect.top) - (bHeight - baseH) * 0.5f;

    context->IASetInputLayout(inputLayout);
    effect->SetTexture(m_texture.Get());
    effect->Apply(context);

    primitiveBatch->Begin();
    VertexPositionTexture v0(Vector3(btnX, btnY, 0.0f), Vector2(0.0f, 0.0f));
    VertexPositionTexture v1(Vector3(btnX + bWidth, btnY, 0.0f), Vector2(1.0f, 0.0f));
    VertexPositionTexture v2(Vector3(btnX, btnY + bHeight, 0.0f), Vector2(0.0f, 1.0f));
    VertexPositionTexture v3(Vector3(btnX + bWidth, btnY + bHeight, 0.0f), Vector2(1.0f, 1.0f));
    primitiveBatch->DrawTriangle(v0, v1, v2);
    primitiveBatch->DrawTriangle(v2, v1, v3);
    primitiveBatch->End();
}