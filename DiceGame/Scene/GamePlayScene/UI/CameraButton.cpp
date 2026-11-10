#include "pch.h"
#include "CameraButton.h"
#include <WICTextureLoader.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// カメラボタン用スタイル定数定義
namespace CameraButtonStyleConstants
{
    constexpr float DEFAULT_MARGIN = 20.0f;     // デフォルトマージン
    constexpr float DEFAULT_TOP_BTN_W = 180.0f; // デフォルトの隣接ボタン幅
    constexpr float DEFAULT_BTN_WIDTH = 130.0f; // デフォルトボタン幅
    constexpr float DEFAULT_BTN_HEIGHT = 60.0f; // デフォルトボタン高さ
    constexpr float HOVER_SCALE = 1.05f;        // ホバー時拡大スケール
}

// 初期化
void CameraButton::Initialize(ID3D11Device* device)
{
    HRESULT hr =
        CreateWICTextureFromFile(device, L"Resources/Textures/CAMERA.jpg", nullptr, m_texture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        CreateWICTextureFromFile(device, L"Resources/Textures/CAMERA.png", nullptr, m_texture.ReleaseAndGetAddressOf());
    }
}

// 配置オブジェクトからの判定座標計算
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
    m_isCustomRectSet = true;
}

// フレーム更新
void CameraButton::Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight,
                          bool isDiceViewerOpen)
{
    (void)screenWidth;
    (void)screenHeight;

    if (isDiceViewerOpen)
    {
        m_isHovered = false;
        return;
    }

    if (!m_isCustomRectSet && m_rect.left == 0 && m_rect.right == 0)
    {
        float margin = CameraButtonStyleConstants::DEFAULT_MARGIN;
        float topBtnW = CameraButtonStyleConstants::DEFAULT_TOP_BTN_W;
        float camX = margin + topBtnW + 15.0f;
        m_rect = {static_cast<LONG>(camX), static_cast<LONG>(margin),
                  static_cast<LONG>(camX + CameraButtonStyleConstants::DEFAULT_BTN_WIDTH),
                  static_cast<LONG>(margin + CameraButtonStyleConstants::DEFAULT_BTN_HEIGHT)};
    }

    m_isHovered = (mouseState.x >= m_rect.left && mouseState.x <= m_rect.right && mouseState.y >= m_rect.top &&
                   mouseState.y <= m_rect.bottom);
}

// 描画処理
void CameraButton::Render(ID3D11DeviceContext* context, PrimitiveBatch<VertexPositionTexture>* primitiveBatch,
                          BasicEffect* effect, ID3D11InputLayout* inputLayout)
{
    if (!m_texture || !primitiveBatch || !effect || !inputLayout)
        return;

    float baseW = static_cast<float>(m_rect.right - m_rect.left);
    float baseH = static_cast<float>(m_rect.bottom - m_rect.top);
    float scale = m_isHovered ? CameraButtonStyleConstants::HOVER_SCALE : 1.0f;

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