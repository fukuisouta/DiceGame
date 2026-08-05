#include "pch.h"
#include "Player.h"
#include <algorithm>
#include <Effects.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Player::SetModelFromData(Microsoft::WRL::ComPtr<ID3D11Device> device,
                              Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, const std::string& modelPath)
{
    
    m_model = nullptr;
    m_primitive = nullptr;

    
    if (!modelPath.empty() && modelPath != "Primitive_Cube")
    {
        try
        {
         
            EffectFactory fx(device.Get());
            fx.SetDirectory(L"Resources/Models");

            std::string fullPathStr = modelPath;
            
            if (fullPathStr.find("Resources/Models/") == std::string::npos)
            {
                fullPathStr = "Resources/Models/" + fullPathStr;
            }

            std::wstring wPath(fullPathStr.begin(), fullPathStr.end());
            m_model = Model::CreateFromCMO(device.Get(), wPath.c_str(), fx);
        }
        catch (...)
        {
            m_model = nullptr;
        }
    }

   
    if (m_model == nullptr)
    {
        if (modelPath == "Primitive_Cube")
        {
            m_primitive = GeometricPrimitive::CreateCube(context.Get(), 1.0f);
        }
        else
        {
           
            m_primitive = GeometricPrimitive::CreateSphere(context.Get(), 1.0f);
        }
    }
}

bool Player::ResolveAABBCollision(const Vector3& objPos, const Vector3& objScale, Vector3& playerPos,
                                  float playerRadius)
{
    // AABB の範囲
    Vector3 minBox = objPos - (objScale * 0.5f);
    Vector3 maxBox = objPos + (objScale * 0.5f);

   
    if (playerPos.y + 1.0f < minBox.y || playerPos.y - 0.5f > maxBox.y)
    {
        return false;
    }

    float closestX = std::max(minBox.x, std::min(playerPos.x, maxBox.x));
    float closestZ = std::max(minBox.z, std::min(playerPos.z, maxBox.z));

    Vector3 diff = Vector3(playerPos.x - closestX, 0.0f, playerPos.z - closestZ);
    float distanceSq = diff.LengthSquared();

    if (distanceSq < (playerRadius * playerRadius))
    {
        float distance = std::sqrt(distanceSq);

        if (distance < 0.0001f)
        {
            diff = Vector3(0.0f, 0.0f, 1.0f);
            distance = 0.0001f;
        }

        float overlap = playerRadius - distance;
        diff /= distance;

       
        playerPos.x += diff.x * overlap;
        playerPos.z += diff.z * overlap;
        return true;
    }

    return false;
}

void Player::Update(float elapsedTime, const Keyboard::State& keyState, const std::vector<PlacedObject>& objects)
{
    Vector3 moveDir = Vector3::Zero;

    // 移動入力
    if (keyState.IsKeyDown(Keyboard::W))
        moveDir.z += 1.0f;
    if (keyState.IsKeyDown(Keyboard::S))
        moveDir.z -= 1.0f;
    if (keyState.IsKeyDown(Keyboard::A))
        moveDir.x += 1.0f;
    if (keyState.IsKeyDown(Keyboard::D))
        moveDir.x -= 1.0f;

    if (moveDir.LengthSquared() > 0.0f)
    {
        moveDir.Normalize();

        m_targetAngle = std::atan2(moveDir.x, moveDir.z);
        m_rotation.y = m_targetAngle;

        Vector3 nextPos = m_position + moveDir * m_moveSpeed * elapsedTime;

       
        float playerRadius = 0.35f * m_scale.x;

       
        Vector3 posX = Vector3(nextPos.x, m_position.y, m_position.z);
        for (const auto& obj : objects)
        {
          
            if (obj.tag == "Door" && obj.isOpen)
            {
                continue;
            }

            ResolveAABBCollision(obj.position, obj.scale, posX, playerRadius);
        }

       
        Vector3 posXZ = Vector3(posX.x, m_position.y, nextPos.z);
        for (const auto& obj : objects)
        {
        
            if (obj.tag == "Door" && obj.isOpen)
            {
                continue;
            }

            ResolveAABBCollision(obj.position, obj.scale, posXZ, playerRadius);
        }

        m_position = posXZ;
    }
}

void Player::Render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, DX11::CommonStates& states, const Matrix& view,
                    const Matrix& projection)
{
    Matrix world = Matrix::CreateScale(m_scale) * Matrix::CreateRotationX(XMConvertToRadians(m_rotation.x)) *
                   Matrix::CreateRotationY(m_rotation.y) * Matrix::CreateRotationZ(XMConvertToRadians(m_rotation.z)) *
                   Matrix::CreateTranslation(m_position);

    if (m_model)
    {
        m_model->Draw(context.Get(), states, world, view, projection);
    }
    else if (m_primitive)
    {
        m_primitive->Draw(world, view, projection, Colors::Green);
    }
}