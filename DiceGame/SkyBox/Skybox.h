#pragma once

#include <d3d11.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <vector>

class Skybox
{
public:
    Skybox();
    ~Skybox() = default;

    // 初期化：DirectXのデバイスと、空の画像（.ddsファイル）のパスを渡す
    bool Initialize(ID3D11Device* device, const wchar_t* cubeMapTexturePath);

    // 描画：毎フレーム呼び出し、カメラの行列と現在位置を渡して空を描画する
    void Render(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewMatrix,
                const DirectX::XMFLOAT4X4& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition);

private:
    // HLSLに送る定数バッファの構造体（16バイト境界アライメント）
    struct ConstantBuffer
    {
        DirectX::XMMATRIX worldViewProjection;
    };

    // 頂点データ構造体（位置情報のみ）
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
    };

    // --- GPU側のバッファや状態管理変数（ComPtrで自動解放） ---
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;   // 頂点バッファ（箱の角データ）
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;    // インデックスバッファ（面を構成する順番）
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer; // 定数バッファ（行列情報）

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader; // 頂点シェーダープログラム
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;   // ピクセルシェーダープログラム
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;   // 頂点データの渡し方（フォーマット）

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV; // 空のキューブマップ画像
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;     // テクスチャのサンプラー設定

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState; // 奥行き判定のルール
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;     // カリング（裏面消去）ルール

    UINT m_indexCount; // 描画するインデックス数（ポリゴンの三角形の数 × 3 = 36）
};