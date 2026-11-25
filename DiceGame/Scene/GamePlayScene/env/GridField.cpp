#include "pch.h"
#include "GridField.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 盤面描画スタイル定数定義
namespace GridFieldStyleConstants
{
    constexpr float FLOOR_Y = -0.01f;         // 床ポリゴンのY座標
    constexpr float TERRITORY_Y = -0.005f;    // 陣地塗りポリゴンのY座標（床とグリッド線の間）
    constexpr float HIGHLIGHT_Y = 0.005f;     // ハイライト領域ポリゴンのY座標
    constexpr float BORDER_Y_OFFSET = 0.001f; // ハイライト外枠線のY座標オフセット
    constexpr float ICON_Y = 0.02f;           // 出目予測アイコンポリゴンのY座標
    constexpr float ICON_PADDING = 0.25f;     // 出目予測アイコンのマス端パディング

    constexpr float LUM_R_WEIGHT = 0.299f; // 輝度計算用赤色ウェイト
    constexpr float LUM_G_WEIGHT = 0.587f; // 輝度計算用緑色ウェイト
    constexpr float LUM_B_WEIGHT = 0.114f; // 輝度計算用青色ウェイト
    constexpr float LUM_THRESHOLD = 0.5f;  // 輝度明暗判定スレッショルド

    constexpr float ICON_ALPHA = 0.65f;   // 出目アイコン表示アルファ値
    constexpr float DEFAULT_ALPHA = 1.0f; // 通常復元用アルファ値
}

// 盤面描画用シェーダー・プリミティブバッチの初期化
void GridField::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize)
{
    m_device = device;
    m_width = width;
    m_height = height;
    m_cellSize = cellSize;

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetVertexColorEnabled(true);

    m_texPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionTexture>>(context);
    m_texEffect = std::make_unique<BasicEffect>(device);
    m_texEffect->SetTextureEnabled(true);
}

// フレーム更新処理
void GridField::Update(float dt)
{
    (void)dt;
}

// 盤面全体のレンダリング処理
void GridField::Render(ID3D11DeviceContext* context, const CommonStates& states, const Matrix& view, const Matrix& proj,
                       int cursorX, int cursorY, int selectedX, int selectedY, bool isReviveMode,
                       const std::vector<MovableTileInfo>& movableTiles, ID3D11ShaderResourceView** faceTextures,
                       int kingAllowedMinY, int kingAllowedMaxY, const std::vector<std::vector<TileColor>>& tileColors)
{
    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(view);
    m_effect->SetProjection(proj);

    context->RSSetState(states.CullNone());
    context->OMSetDepthStencilState(states.DepthDefault(), 0);
    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);

    m_effect->Apply(context);

    void const* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    m_device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                shaderByteCode, byteCodeLength, inputLayout.GetAddressOf());
    context->IASetInputLayout(inputLayout.Get());

    float totalWidth = m_width * m_cellSize;
    float totalHeight = m_height * m_cellSize;
    float startX = -totalWidth * GridFieldConstants::HALF_SCALE;
    float startZ = totalHeight * GridFieldConstants::HALF_SCALE;
    float endX = totalWidth * GridFieldConstants::HALF_SCALE;
    float endZ = -totalHeight * GridFieldConstants::HALF_SCALE;

    m_primitiveBatch->Begin();

    // 1. 床面（平面ポリゴン）の描画
    float floorY = GridFieldStyleConstants::FLOOR_Y;
    VertexPositionColor f0(Vector3(startX, floorY, startZ), m_floorColor);
    VertexPositionColor f1(Vector3(endX, floorY, startZ), m_floorColor);
    VertexPositionColor f2(Vector3(startX, floorY, endZ), m_floorColor);
    VertexPositionColor f3(Vector3(endX, floorY, endZ), m_floorColor);

    m_primitiveBatch->DrawTriangle(f0, f2, f1);
    m_primitiveBatch->DrawTriangle(f1, f2, f3);

    // 1.5. 陣地塗りカラー（Player1: 青 / Player2: 赤）の描画
    if (!tileColors.empty())
    {
        const Color p1TerritoryColor(0.15f, 0.45f, 0.95f, 0.75f);
        const Color p2TerritoryColor(0.95f, 0.22f, 0.25f, 0.75f);
        float territoryY = GridFieldStyleConstants::TERRITORY_Y;

        int maxRows = std::min(m_height, static_cast<int>(tileColors.size()));
        for (int y = 0; y < maxRows; ++y)
        {
            int maxCols = std::min(m_width, static_cast<int>(tileColors[y].size()));
            for (int x = 0; x < maxCols; ++x)
            {
                TileColor ownerColor = tileColors[y][x];
                if (ownerColor == TileColor::None)
                    continue;

                Color fillColor = (ownerColor == TileColor::Player1) ? p1TerritoryColor : p2TerritoryColor;

                float minX = startX + x * m_cellSize;
                float maxX = minX + m_cellSize;
                float maxZ = startZ - y * m_cellSize;
                float minZ = maxZ - m_cellSize;

                VertexPositionColor t0(Vector3(minX, territoryY, maxZ), fillColor);
                VertexPositionColor t1(Vector3(maxX, territoryY, maxZ), fillColor);
                VertexPositionColor t2(Vector3(maxX, territoryY, minZ), fillColor);
                VertexPositionColor t3(Vector3(minX, territoryY, minZ), fillColor);

                m_primitiveBatch->DrawTriangle(t0, t3, t1);
                m_primitiveBatch->DrawTriangle(t1, t3, t2);
            }
        }
    }

    // 2. グリッド格子線の色判定および描画
    float luminance = m_floorColor.R() * GridFieldStyleConstants::LUM_R_WEIGHT +
                      m_floorColor.G() * GridFieldStyleConstants::LUM_G_WEIGHT +
                      m_floorColor.B() * GridFieldStyleConstants::LUM_B_WEIGHT;

    Color gridColor = (luminance > GridFieldStyleConstants::LUM_THRESHOLD) ? Color(0.2f, 0.2f, 0.2f, 1.0f)
                                                                           : Color(0.8f, 0.8f, 0.8f, 1.0f);

    for (int x = 0; x <= m_width; ++x)
    {
        float currentX = startX + x * m_cellSize;
        VertexPositionColor v1(Vector3(currentX, 0.0f, startZ), gridColor);
        VertexPositionColor v2(Vector3(currentX, 0.0f, endZ), gridColor);
        m_primitiveBatch->DrawLine(v1, v2);
    }

    for (int y = 0; y <= m_height; ++y)
    {
        float currentZ = startZ - y * m_cellSize;
        VertexPositionColor v1(Vector3(startX, 0.0f, currentZ), gridColor);
        VertexPositionColor v2(Vector3(endX, 0.0f, currentZ), gridColor);
        m_primitiveBatch->DrawLine(v1, v2);
    }

    auto DrawCellFill = [&](int gridX, int gridY, const Color& fillColor, const Color& borderColor)
    {
        if (gridX < 0 || gridX >= m_width || gridY < 0 || gridY >= m_height)
            return;

        float minX = startX + gridX * m_cellSize;
        float maxX = minX + m_cellSize;
        float maxZ = startZ - gridY * m_cellSize;
        float minZ = maxZ - m_cellSize;
        float highlightY = GridFieldStyleConstants::HIGHLIGHT_Y;

        VertexPositionColor c0(Vector3(minX, highlightY, maxZ), fillColor);
        VertexPositionColor c1(Vector3(maxX, highlightY, maxZ), fillColor);
        VertexPositionColor c2(Vector3(maxX, highlightY, minZ), fillColor);
        VertexPositionColor c3(Vector3(minX, highlightY, minZ), fillColor);

        m_primitiveBatch->DrawTriangle(c0, c3, c1);
        m_primitiveBatch->DrawTriangle(c1, c3, c2);

        if (borderColor.A() > 0.0f)
        {
            float borderY = highlightY + GridFieldStyleConstants::BORDER_Y_OFFSET;
            VertexPositionColor b0(Vector3(minX, borderY, maxZ), borderColor);
            VertexPositionColor b1(Vector3(maxX, borderY, maxZ), borderColor);
            VertexPositionColor b2(Vector3(maxX, borderY, minZ), borderColor);
            VertexPositionColor b3(Vector3(minX, borderY, minZ), borderColor);

            m_primitiveBatch->DrawLine(b0, b1);
            m_primitiveBatch->DrawLine(b1, b2);
            m_primitiveBatch->DrawLine(b2, b3);
            m_primitiveBatch->DrawLine(b3, b0);
        }
    };

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

    Color moveFillColor = Color(0.0f, 0.95f, 0.5f, 0.40f);
    Color moveBorderColor = Color(0.2f, 1.0f, 0.7f, 1.00f);

    for (const auto& tile : movableTiles)
    {
        DrawCellFill(tile.gridX, tile.gridY, moveFillColor, moveBorderColor);
    }

    if (isReviveMode)
    {
        Color reviveFill = Color(0.8f, 0.2f, 1.0f, 0.45f);
        Color reviveBorder = Color(1.0f, 0.4f, 1.0f, 0.95f);
        for (int x = 0; x < m_width; ++x)
        {
            DrawCellFill(x, GridFieldConstants::REVIVE_ROW_1, reviveFill, reviveBorder);
            DrawCellFill(x, GridFieldConstants::REVIVE_ROW_2, reviveFill, reviveBorder);
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

    // 3. 移動先マス上への出目予測アイコンテクスチャ描画
    if (faceTextures && !movableTiles.empty())
    {
        m_texEffect->SetWorld(Matrix::Identity);
        m_texEffect->SetView(view);
        m_texEffect->SetProjection(proj);

        void const* texByteCode = nullptr;
        size_t texByteCodeLength = 0;
        m_texEffect->GetVertexShaderBytecode(&texByteCode, &texByteCodeLength);

        Microsoft::WRL::ComPtr<ID3D11InputLayout> texInputLayout;
        m_device->CreateInputLayout(VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount,
                                    texByteCode, texByteCodeLength, texInputLayout.GetAddressOf());
        context->IASetInputLayout(texInputLayout.Get());

        float padding = GridFieldStyleConstants::ICON_PADDING;

        for (const auto& tile : movableTiles)
        {
            int face = tile.expectedFace;
            if (face < GridFieldConstants::MIN_DICE_FACE || face > GridFieldConstants::MAX_DICE_FACE ||
                !faceTextures[face])
                continue;

            context->OMSetDepthStencilState(states.DepthNone(), 0);

            m_texEffect->SetTexture(faceTextures[face]);
            m_texEffect->SetAlpha(GridFieldStyleConstants::ICON_ALPHA);
            m_texEffect->Apply(context);

            float texY = GridFieldStyleConstants::ICON_Y;
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

        m_texEffect->SetAlpha(GridFieldStyleConstants::DEFAULT_ALPHA);
        context->OMSetDepthStencilState(states.DepthDefault(), 0);
    }
}