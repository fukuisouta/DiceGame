#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include <CommonStates.h>
#include <memory>
#include <string>

// ダイス単体の状態管理、挙動、描画を行うクラス
class Dice
{
public:
    Dice() = default;
    virtual ~Dice() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath);

    void SetPosition(const DirectX::SimpleMath::Vector3& pos)
    {
        m_position = pos;
    }
    DirectX::SimpleMath::Vector3 GetPosition() const
    {
        return m_position;
    }

    void SetRotation(const DirectX::SimpleMath::Quaternion& rot)
    {
        m_rotation = rot;
    }
    DirectX::SimpleMath::Quaternion GetRotation() const
    {
        return m_rotation;
    }

    void SetScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_scale = scale;
    }
    DirectX::SimpleMath::Vector3 GetScale() const
    {
        return m_scale;
    }

    void SetCollisionScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_collisionScale = scale;
    }
    DirectX::SimpleMath::Vector3 GetCollisionScale() const
    {
        return m_collisionScale;
    }

    // ワールド行列を取得
    DirectX::SimpleMath::Matrix GetWorldMatrix() const
    {
        return DirectX::SimpleMath::Matrix::CreateScale(m_scale) *
               DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_rotation) *
               DirectX::SimpleMath::Matrix::CreateTranslation(m_position);
    }

    void Roll(const DirectX::SimpleMath::Vector3& initialImpulse);
    bool IsSettled() const
    {
        return m_isSettled;
    }

    void SetEmissiveColor(const DirectX::SimpleMath::Vector3& color)
    {
        m_emissiveColor = color;
    }
    DirectX::SimpleMath::Vector3 GetEmissiveColor() const
    {
        return m_emissiveColor;
    }

    virtual void Update(float dt);

    // 4引数版 Render
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj) const;

    // 5引数版 Render (ワールド行列を直接受けるオーバーロード)
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view,
                        const DirectX::SimpleMath::Matrix& proj) const;

    int GetUpwardFace() const;

private:
    DirectX::SimpleMath::Vector3 m_position = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Quaternion m_rotation = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Vector3 m_scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
    DirectX::SimpleMath::Vector3 m_collisionScale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

    DirectX::SimpleMath::Vector3 m_velocity = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 m_angularVelocity = DirectX::SimpleMath::Vector3::Zero;

    bool m_isSettled = false;
    DirectX::SimpleMath::Vector3 m_emissiveColor = DirectX::SimpleMath::Vector3::Zero;

    std::unique_ptr<DirectX::Model> m_model;
};