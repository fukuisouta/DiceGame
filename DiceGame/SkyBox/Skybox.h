#pragma once

#include <d3d11.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <vector>

// スカイボックス定数定義
namespace SkyboxConstants
{
    constexpr UINT INDEX_COUNT_CUBE = 36; // スカイボックス立方体の総インデックス数 (6面 × 2三角形 × 3頂点)
}

// 3D空間の遠景・空（スカイボックス）を描画・管理するクラス
class Skybox
{
public:
    Skybox();
    ~Skybox() = default;

    // スカイボックスを初期化する関数
    // 引数 device: Direct3D11デバイス, cubeMapTexturePath: キューブマップテクスチャファイルパス
    // 戻り値: 初期化成功時 true
    bool Initialize(ID3D11Device* device, const wchar_t* cubeMapTexturePath);

    // スカイボックスを描画する関数
    // 引数 context: デバイスコンテキスト, viewMatrix: ビュー行列, projectionMatrix: プロジェクション行列,
    // cameraPosition: カメラ位置座標
    void Render(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewMatrix,
                const DirectX::XMFLOAT4X4& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition);

private:
    // HLSLに送る定数バッファ構造体 (16バイトアライメント)
    struct ConstantBuffer
    {
        DirectX::XMMATRIX worldViewProjection; // ワールド・ビュー・プロジェクション結合行列
    };

    // 頂点データ構造体
    struct Vertex
    {
        DirectX::XMFLOAT3 position; // 頂点位置座標
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;   // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;    // インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer; // 定数バッファ

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader; // 頂点シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;   // ピクセルシェーダー
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;   // 入力レイアウト

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV; // キューブマップテクスチャリソース
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;     // サンプラーステート

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState; // 深度ステンシルステート
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;     // ラスタライザーステート

    UINT m_indexCount; // インデックス数
};