#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <CommonStates.h>
#include <algorithm>
#include "Framework/ObjectData.h"

// ============================================================================
// カメラ切替UIボタン管理クラス
// ============================================================================
class CameraButton
{
public:
    CameraButton() = default;
    ~CameraButton() = default;

    // テクスチャおよび初期化
    void Initialize(ID3D11Device* device);

    // JSON配置オブジェクトからの座標領域設定
    void SetFromPlacedObject(const PlacedObject& obj, float orthoWidth, float orthoHeight, float screenWidth,
                             float screenHeight);

    // 毎フレームの更新（ホバー判定）
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight, bool isDiceViewerOpen);

    // 描画処理
    void Render(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* primitiveBatch,
                DirectX::BasicEffect* effect, ID3D11InputLayout* inputLayout);

    // --- 状態取得 ---
    bool IsHovered() const
    {
        return m_isHovered;
    }
    const RECT& GetRect() const
    {
        return m_rect;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;
    RECT m_rect = {0, 0, 0, 0};
    bool m_isHovered = false;
};