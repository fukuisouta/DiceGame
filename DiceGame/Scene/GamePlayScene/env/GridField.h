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

// 盤面描画用定数定義
namespace GridFieldConstants
{
    constexpr int REVIVE_ROW_1 = 6;    // 蘇生エリアY格子行1
    constexpr int REVIVE_ROW_2 = 7;    // 蘇生エリアY格子行2
    constexpr int MIN_DICE_FACE = 1;   // ダイスの出目最小値
    constexpr int MAX_DICE_FACE = 6;   // ダイスの出目最大値
    constexpr float HALF_SCALE = 0.5f; // 中心オフセット計算用倍率
}

// 移動可能マスの情報構造体
struct MovableTileInfo
{
    int gridX;        // 格子X座標
    int gridY;        // 格子Y座標
    int expectedFace; // そのマスに移動した際の予想出目（1～6）
};

// 盤面（グリッドフィールド）の床、グリッド線、ハイライト領域、予想出目描画クラス
class GridField
{
public:
    GridField() = default;
    ~GridField() = default;

    // グリッド盤面の初期化を行う関数
    // 引数 device: Direct3D11デバイス, context: デバイスコンテキスト, width: 横マス数, height: 縦マス数, cellSize:
    // 1マスの幅
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize);

    // 毎フレームの盤面アニメーション更新を行う関数
    // 引数 dt: デルタタイム
    void Update(float dt);

    // 盤面（床・格子線・ハイライトマス・移動可能マス・予想出目アイコン）を描画する関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, view: ビュー行列, proj: プロジェクション行列,
    //      cursorX, cursorY: カーソル格子位置, selectedX, selectedY: 選択中格子位置, isReviveMode:
    //      蘇生選択モードフラグ, movableTiles: 移動可能マスリスト, faceTextures: 出目テクスチャ配列, kingAllowedMinY,
    //      kingAllowedMaxY: 王の移動許可Y範囲
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states,
                const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj, int cursorX,
                int cursorY, int selectedX, int selectedY, bool isReviveMode = false,
                const std::vector<MovableTileInfo>& movableTiles = {},
                ID3D11ShaderResourceView** faceTextures = nullptr, int kingAllowedMinY = -1, int kingAllowedMaxY = -1);

    // 1マスのサイズ（ワールド単位）を取得する関数
    // 戻り値: マス幅
    float GetCellSize() const
    {
        return m_cellSize;
    }

    // 床の色（クリアカラー/ポリゴン色）を設定する関数
    // 引数 color: 設定する背景色
    void SetFloorColor(const DirectX::SimpleMath::Color& color)
    {
        m_floorColor = color;
    }

    // 現在設定されている床の色を取得する関数
    // 戻り値: 床カラー
    DirectX::SimpleMath::Color GetFloorColor() const
    {
        return m_floorColor;
    }

private:
    ID3D11Device* m_device = nullptr; // Direct3D11デバイス参照
    int m_width = 0;                  // 盤面の横格子数
    int m_height = 0;                 // 盤面の縦格子数
    float m_cellSize = 0.0f;          // 1マスのワールドサイズ

    DirectX::SimpleMath::Color m_floorColor = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 1.0f); // 描画床カラー

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>
        m_primitiveBatch;                           // 単色ポリゴン・ライン描画バッチ
    std::unique_ptr<DirectX::BasicEffect> m_effect; // 単色プリミティブ用エフェクト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>>
        m_texPrimitiveBatch;                           // テクスチャポリゴン描画バッチ
    std::unique_ptr<DirectX::BasicEffect> m_texEffect; // テクスチャプリミティブ用エフェクト
};