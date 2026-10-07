#include "pch.h"
#include "GridField.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void GridField::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize)
{
    m_device = device;
    m_width = width;
    m_height = height;
    m_cellSize = cellSize;

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetVertexColorEnabled(true);
}

void GridField::Update(float dt)
{
}

void GridField::Render(ID3D11DeviceContext* context, const CommonStates& states, const Matrix& view, const Matrix& proj,
                       int cursorX, int cursorY, int selectedX, int selectedY, bool isReviveMode)
{
    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(view);
    m_effect->SetProjection(proj);

    context->RSSetState(states.CullNone());
    context->OMSetBlendState(states.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.DepthDefault(), 0);

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

    float floorY = -0.01f;

    VertexPositionColor f0(Vector3(startX, floorY, startZ), m_floorColor);
    VertexPositionColor f1(Vector3(endX, floorY, startZ), m_floorColor);
    VertexPositionColor f2(Vector3(startX, floorY, endZ), m_floorColor);
    VertexPositionColor f3(Vector3(endX, floorY, endZ), m_floorColor);

    m_primitiveBatch->DrawTriangle(f0, f2, f1);
    m_primitiveBatch->DrawTriangle(f1, f2, f3);

    float luminance = m_floorColor.R() * 0.299f + m_floorColor.G() * 0.587f + m_floorColor.B() * 0.114f;
    Color gridColor = (luminance > 0.5f) ? Color(0.2f, 0.2f, 0.2f, 1.0f) : Color(0.8f, 0.8f, 0.8f, 1.0f);

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

    auto DrawCellHighlight = [&](int gridX, int gridY, const Color& color)
    {
        if (gridX < 0 || gridX >= m_width || gridY < 0 || gridY >= m_height)
            return;

        float minX = startX + gridX * m_cellSize;
        float maxX = minX + m_cellSize;
        float maxZ = startZ - gridY * m_cellSize;
        float minZ = maxZ - m_cellSize;
        float highlightY = 0.005f;

        VertexPositionColor c0(Vector3(minX, highlightY, maxZ), color);
        VertexPositionColor c1(Vector3(maxX, highlightY, maxZ), color);
        VertexPositionColor c2(Vector3(maxX, highlightY, minZ), color);
        VertexPositionColor c3(Vector3(minX, highlightY, minZ), color);

        m_primitiveBatch->DrawLine(c0, c1);
        m_primitiveBatch->DrawLine(c1, c2);
        m_primitiveBatch->DrawLine(c2, c3);
        m_primitiveBatch->DrawLine(c3, c0);
    };

    // 蘇生選択モード時は手前2行（Y = 6, 7）を全ハイライト
    if (isReviveMode)
    {
        Color reviveZoneColor = Color(0.8f, 0.2f, 1.0f, 1.0f);
        for (int x = 0; x < m_width; ++x)
        {
            DrawCellHighlight(x, 6, reviveZoneColor);
            DrawCellHighlight(x, 7, reviveZoneColor);
        }
    }
    else
    {
        if (selectedX != -1 && selectedY != -1)
        {
            DrawCellHighlight(selectedX, selectedY, Color(1.0f, 0.8f, 0.0f, 1.0f));
        }
        DrawCellHighlight(cursorX, cursorY, Color(0.0f, 0.8f, 1.0f, 1.0f));
    }

    m_primitiveBatch->End();
}