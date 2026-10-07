#pragma once

#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>

class GridField
{
public:
    GridField() = default;
    ~GridField() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height, float cellSize);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states,
                const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj, int cursorX,
                int cursorY, int selectedX, int selectedY, bool isReviveMode = false);

    float GetCellSize() const
    {
        return m_cellSize;
    }

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
    int m_width = 0;
    int m_height = 0;
    float m_cellSize = 0.0f;

    DirectX::SimpleMath::Color m_floorColor = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 1.0f);

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
};