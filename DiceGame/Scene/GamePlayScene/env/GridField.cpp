#include "pch.h"
#include "GridField.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 盤面描画用シェーダー・プリミティブバッチの初期化
void GridField::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize)
{
    m_device = device;
    m_width = width;
    m_height = height;
    m_cellSize = cellSize;

    // カラープリミティブ（床・枠線用）のセットアップ
    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetVertexColorEnabled(true);

    // テクスチャプリミティブ（移動先予想出目用）のセットアップ
    m_texPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionTexture>>(context);
    m_texEffect = std::make_unique<BasicEffect>(device);
    m_texEffect->SetTextureEnabled(true);
}

void GridField::Update(float dt)
{
    (void)dt;
}

// 盤面全体のレンダリング処理
void GridField::Render(ID3D11DeviceContext* context, const CommonStates& states, const Matrix& view, const Matrix& proj,
                       int cursorX, int cursorY, int selectedX, int selectedY, bool isReviveMode,
                       const std::vector<MovableTileInfo>& movableTiles, ID3D11ShaderResourceView** faceTextures,
                       int kingAllowedMinY, int kingAllowedMaxY)
{
    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(view);
    m_effect->SetProjection(proj);

    context->RSSetState(states.CullNone());
    context->OMSetDepthStencilState(states.DepthDefault(), 0);
    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);

    m_effect->Apply(context);

    void const* shaderByteCode;
    size_t byteCodeLength;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    m_device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                shaderByteCode, byteCodeLength, inputLayout.GetAddressOf());
    context->IASetInputLayout(inputLayout.Get());

    float totalWidth = m_width * m_cellSize;
    float totalHeight = m_height * m_cellSize;
    float startX = -totalWidth * 0.5f;
    float startZ = totalHeight * 0.5f;
    float endX = totalWidth * 0.5f;
    float endZ = -totalHeight * 0.5f;

    m_primitiveBatch->Begin();

    // 1. 床面（平面ポリゴン）の描画
    float floorY = -0.01f;
    VertexPositionColor f0(Vector3(startX, floorY, startZ), m_floorColor);
    VertexPositionColor f1(Vector3(endX, floorY, startZ), m_floorColor);
    VertexPositionColor f2(Vector3(startX, floorY, endZ), m_floorColor);
    VertexPositionColor f3(Vector3(endX, floorY, endZ), m_floorColor);

    m_primitiveBatch->DrawTriangle(f0, f2, f1);
    m_primitiveBatch->DrawTriangle(f1, f2, f3);

    // 2. 床の明るさに応じて視認性の良いグリッド格子線の色を自動計算
    float luminance = m_floorColor.R() * 0.299f + m_floorColor.G() * 0.587f + m_floorColor.B() * 0.114f;
    Color gridColor = (luminance > 0.5f) ? Color(0.2f, 0.2f, 0.2f, 1.0f) : Color(0.8f, 0.8f, 0.8f, 1.0f);

    // 縦グリッド線の描画
    for (int x = 0; x <= m_width; ++x)
    {
        float currentX = startX + x * m_cellSize;
        VertexPositionColor v1(Vector3(currentX, 0.0f, startZ), gridColor);
        VertexPositionColor v2(Vector3(currentX, 0.0f, endZ), gridColor);
        m_primitiveBatch->DrawLine(v1, v2);
    }

    // 横グリッド線の描画
    for (int y = 0; y <= m_height; ++y)
    {
        float currentZ = startZ - y * m_cellSize;
        VertexPositionColor v1(Vector3(startX, 0.0f, currentZ), gridColor);
        VertexPositionColor v2(Vector3(endX, 0.0f, currentZ), gridColor);
        m_primitiveBatch->DrawLine(v1, v2);
    }

    // 特定のマス目を塗りつぶし・枠線描画するラムダ関数
    auto DrawCellFill = [&](int gridX, int gridY, const Color& fillColor, const Color& borderColor)
    {
        if (gridX < 0 || gridX >= m_width || gridY < 0 || gridY >= m_height)
            return;

        float minX = startX + gridX * m_cellSize;
        float maxX = minX + m_cellSize;
        float maxZ = startZ - gridY * m_cellSize;
        float minZ = maxZ - m_cellSize;
        float highlightY = 0.005f;

        VertexPositionColor c0(Vector3(minX, highlightY, maxZ), fillColor);
        VertexPositionColor c1(Vector3(maxX, highlightY, maxZ), fillColor);
        VertexPositionColor c2(Vector3(maxX, highlightY, minZ), fillColor);
        VertexPositionColor c3(Vector3(minX, highlightY, minZ), fillColor);

        m_primitiveBatch->DrawTriangle(c0, c3, c1);
        m_primitiveBatch->DrawTriangle(c1, c3, c2);

        // 外枠線の描画
        if (borderColor.A() > 0.0f)
        {
            VertexPositionColor b0(Vector3(minX, highlightY + 0.001f, maxZ), borderColor);
            VertexPositionColor b1(Vector3(maxX, highlightY + 0.001f, maxZ), borderColor);
            VertexPositionColor b2(Vector3(maxX, highlightY + 0.001f, minZ), borderColor);
            VertexPositionColor b3(Vector3(minX, highlightY + 0.001f, minZ), borderColor);

            m_primitiveBatch->DrawLine(b0, b1);
            m_primitiveBatch->DrawLine(b1, b2);
            m_primitiveBatch->DrawLine(b2, b3);
            m_primitiveBatch->DrawLine(b3, b0);
        }
    };

    // 王（King）の移動可能範囲エリアのハイライト表示
    if (kingAllowedMinY >= 0 && kingAllowedMaxY >= 0)
    {
        Color zoneFill = Color(0.0f, 0.8f, 0.4f, 0.20f);
        Color zoneBorder = Color(0.0f, 0.9f, 0.5f, 0.60f);

        for (int y = kingAllowedMinY; y <= kingAllowedMaxY; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                DrawCellFill(x, y, zoneFill, zoneBorder);
            }
        }
    }

    // 移動可能マスのハイライト描画
    Color moveFillColor = Color(0.0f, 0.95f, 0.5f, 0.40f);
    Color moveBorderColor = Color(0.2f, 1.0f, 0.7f, 1.00f);

    for (const auto& tile : movableTiles)
    {
        DrawCellFill(tile.gridX, tile.gridY, moveFillColor, moveBorderColor);
    }

    // 蘇生選択モード時の自陣エリア描画、および通常選択・カーソルハイライト描画
    if (isReviveMode)
    {
        Color reviveFill = Color(0.8f, 0.2f, 1.0f, 0.45f);
        Color reviveBorder = Color(1.0f, 0.4f, 1.0f, 0.95f);
        for (int x = 0; x < m_width; ++x)
        {
            DrawCellFill(x, 6, reviveFill, reviveBorder);
            DrawCellFill(x, 7, reviveFill, reviveBorder);
        }
    }
    else
    {
        if (selectedX != -1 && selectedY != -1)
        {
            DrawCellFill(selectedX, selectedY, Color(1.0f, 0.8f, 0.0f, 0.55f), Color(1.0f, 0.9f, 0.2f, 1.0f));
        }
        if (cursorX != -1 && cursorY != -1)
        {
            DrawCellFill(cursorX, cursorY, Color(0.0f, 0.8f, 1.0f, 0.35f), Color(0.3f, 0.9f, 1.0f, 1.0f));
        }
    }

    m_primitiveBatch->End();

    // 3. 移動先マス上への「予想上面出目」アイコンテクスチャ描画
    if (faceTextures && !movableTiles.empty())
    {
        m_texEffect->SetWorld(Matrix::Identity);
        m_texEffect->SetView(view);
        m_texEffect->SetProjection(proj);

        void const* texByteCode;
        size_t texByteCodeLength;
        m_texEffect->GetVertexShaderBytecode(&texByteCode, &texByteCodeLength);

        Microsoft::WRL::ComPtr<ID3D11InputLayout> texInputLayout;
        m_device->CreateInputLayout(VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount,
                                    texByteCode, texByteCodeLength, texInputLayout.GetAddressOf());
        context->IASetInputLayout(texInputLayout.Get());

        float padding = 0.25f;

        for (const auto& tile : movableTiles)
        {
            int face = tile.expectedFace;
            if (face < 1 || face > 6 || !faceTextures[face])
                continue;

            // コマモデルに埋もれず視認できるよう一時的に深度テストを無効化
            context->OMSetDepthStencilState(states.DepthNone(), 0);

            // アルファ値（半透明度）を設定してテクスチャを適用
            m_texEffect->SetTexture(faceTextures[face]);
            m_texEffect->SetAlpha(0.65f);
            m_texEffect->Apply(context);

            float texY = 0.02f;
            float minX = startX + tile.gridX * m_cellSize + padding;
            float maxX = startX + (tile.gridX + 1) * m_cellSize - padding;
            float maxZ = startZ - tile.gridY * m_cellSize - padding;
            float minZ = startZ - (tile.gridY + 1) * m_cellSize + padding;

            m_texPrimitiveBatch->Begin();

            VertexPositionTexture t0(Vector3(minX, texY, maxZ), Vector2(0.0f, 0.0f));
            VertexPositionTexture t1(Vector3(maxX, texY, maxZ), Vector2(1.0f, 0.0f));
            VertexPositionTexture t2(Vector3(minX, texY, minZ), Vector2(0.0f, 1.0f));
            VertexPositionTexture t3(Vector3(maxX, texY, minZ), Vector2(1.0f, 1.0f));

            m_texPrimitiveBatch->DrawTriangle(t0, t1, t2);
            m_texPrimitiveBatch->DrawTriangle(t2, t1, t3);

            m_texPrimitiveBatch->End();
        }

        // 描画完了後にアルファ値と深度ステートを既定値へ復元
        m_texEffect->SetAlpha(1.0f);
        context->OMSetDepthStencilState(states.DepthDefault(), 0);
    }
}