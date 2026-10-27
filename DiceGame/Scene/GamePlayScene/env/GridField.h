#pragma once

#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>

// 移動可能マスの情報構造体（座標および移動後の予想ダイス上面の出目）
struct MovableTileInfo
{
    int gridX;        // マス目X座標
    int gridY;        // マス目Y座標
    int expectedFace; // そのマスに移動した際の予想出目（1～6）
};

// ============================================================================
// 盤面（グリッドフィールド）の床、グリッド線、ハイライト領域、予想出目描画クラス
// ============================================================================
class GridField
{
public:
    GridField() = default;
    ~GridField() = default;

    // グリッド盤面の初期化（サイズ・セル大きさ・バッチリソース作成）
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize);
    void Update(float dt);

    // 盤面（床・格子線・ハイライトマス・移動可能マス・予想出目アイコン）のレンダリング
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states,
                const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj, int cursorX,
                int cursorY, int selectedX, int selectedY, bool isReviveMode = false,
                const std::vector<MovableTileInfo>& movableTiles = {},
                ID3D11ShaderResourceView** faceTextures = nullptr, int kingAllowedMinY = -1, int kingAllowedMaxY = -1);

    // 1セルのサイズを取得
    float GetCellSize() const
    {
        return m_cellSize;
    }

    // 床カラーのセットおよび取得
    void SetFloorColor(const DirectX::SimpleMath::Color& color)
    {
        m_floorColor = color;
    }
    DirectX::SimpleMath::Color GetFloorColor() const
    {
        return m_floorColor;
    }

private:
    ID3D11Device* m_device = nullptr;
    int m_width = 0;         // 横マス数
    int m_height = 0;        // 縦マス数
    float m_cellSize = 0.0f; // 1マスのサイズ (ワールド単位)

    DirectX::SimpleMath::Color m_floorColor = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 1.0f); // 床カラー

    // 色付きプリミティブ用描画バッチ（床・枠線・ハイライト用）
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;

    // テクスチャ付きプリミティブ用描画バッチ（出目画像用）
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_texPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_texEffect;
};