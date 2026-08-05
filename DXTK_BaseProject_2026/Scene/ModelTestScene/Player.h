#pragma once

#include <string>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <Model.h>
#include <GeometricPrimitive.h>
#include <Keyboard.h>
#include <CommonStates.h>
#include "Scene/EditorScene/ObjectData.h"

class Player
{
public:
    Player() = default;
    ~Player() = default;

    void SetPosition(const DirectX::SimpleMath::Vector3& position)
    {
        m_position = position;
    }
    void SetRotation(const DirectX::SimpleMath::Vector3& rotation)
    {
        m_rotation = rotation;
    }
    void SetScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_scale = scale;
    }

    const DirectX::SimpleMath::Vector3& GetPosition() const
    {
        return m_position;
    }
    const DirectX::SimpleMath::Vector3& GetRotation() const
    {
        return m_rotation;
    }
    const DirectX::SimpleMath::Vector3& GetScale() const
    {
        return m_scale;
    }

    void SetModelFromData(Microsoft::WRL::ComPtr<ID3D11Device> device,
                          Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, const std::string& modelPath);

    // 衝突解決関数
    bool ResolveAABBCollision(const DirectX::SimpleMath::Vector3& objPos, const DirectX::SimpleMath::Vector3& objScale,
                              DirectX::SimpleMath::Vector3& playerPos, float playerRadius);

    // 引数3つの Update
    void Update(float elapsedTime, const DirectX::Keyboard::State& keyState, const std::vector<PlacedObject>& objects);

    void Render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, DirectX::CommonStates& states,
                const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection);

private:
    DirectX::SimpleMath::Vector3 m_position = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_rotation = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_scale = {1.0f, 1.0f, 1.0f};

    std::shared_ptr<DirectX::Model> m_model;
    std::shared_ptr<DirectX::GeometricPrimitive> m_primitive;

    float m_moveSpeed = 5.0f;
    float m_targetAngle = 0.0f;
};