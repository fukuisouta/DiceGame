#include "pch.h"
#include "Dice.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void Dice::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath)
{
    EffectFactory fx(device);
    fx.SetDirectory(L"Resources/Models");

    std::wstring wPath(modelPath.begin(), modelPath.end());
    try
    {
        m_model = Model::CreateFromCMO(device, wPath.c_str(), fx);
    }
    catch (...)
    {
    }

    m_position = Vector3(0.0f, 1.0f, 0.0f);
    m_rotation = Quaternion::Identity;
    m_scale = Vector3(1.0f, 1.0f, 1.0f);
    m_collisionScale = Vector3(1.0f, 1.0f, 1.0f);
    m_velocity = Vector3::Zero;
    m_angularVelocity = Vector3::Zero;
    m_isSettled = false;
    m_emissiveColor = Vector3::Zero;
}

void Dice::Roll(const Vector3& initialImpulse)
{
    m_velocity = initialImpulse * 2.5f;
    m_velocity.y += 5.0f;

    m_angularVelocity = Vector3(static_cast<float>(rand() % 31 - 15), static_cast<float>(rand() % 31 - 15),
                                static_cast<float>(rand() % 31 - 15));

    m_isSettled = false;
}

void Dice::Update(float dt)
{
    if (m_isSettled)
    {
        return;
    }

    m_velocity.y -= 9.8f * dt;
    Vector3 nextPos = m_position + m_velocity * dt;
    float floorY = 1.0f;

    if (nextPos.y <= floorY)
    {
        nextPos.y = floorY;

        if (m_velocity.y < 0.0f)
        {
            m_velocity.y = -m_velocity.y * 0.2f;
            if (std::abs(m_velocity.y) < 0.2f)
            {
                m_velocity.y = 0.0f;
            }
        }

        m_velocity.x *= 0.85f;
        m_velocity.z *= 0.85f;
        m_angularVelocity *= 0.85f;
    }

    m_position = nextPos;

    if (m_angularVelocity.LengthSquared() > 0.001f)
    {
        Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(m_angularVelocity.y * dt, m_angularVelocity.x * dt,
                                                                 m_angularVelocity.z * dt);
        m_rotation = m_rotation * deltaRot;
        m_rotation.Normalize();
    }

    if (m_position.y <= floorY + 0.05f && m_velocity.LengthSquared() < 0.2f && m_angularVelocity.LengthSquared() < 0.2f)
    {
        m_velocity = Vector3::Zero;
        m_angularVelocity = Vector3::Zero;
        m_position.y = floorY;
        m_isSettled = true;
    }
}

// 4引数版：自身のトランスフォームからワールド行列を作って描画
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& view, const Matrix& proj) const
{
    Render(context, states, GetWorldMatrix(), view, proj);
}

// 5引数版：指定されたワールド行列で描画
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& world, const Matrix& view,
                  const Matrix& proj) const
{
    if (!m_model)
        return;

    m_model->UpdateEffects(
        [this](IEffect* effect)
        {
            if (auto basicEffect = dynamic_cast<BasicEffect*>(effect))
            {
                basicEffect->SetEmissiveColor(m_emissiveColor);
            }
        });

    m_model->Draw(context, states, world, view, proj);
}

int Dice::GetUpwardFace() const
{
    struct FaceVector
    {
        int number;
        Vector3 localDir;
    };

    static const FaceVector faces[] = {
        {3, Vector3(0.0f, 1.0f, 0.0f)},  {4, Vector3(0.0f, -1.0f, 0.0f)}, {2, Vector3(0.0f, 0.0f, 1.0f)},
        {5, Vector3(0.0f, 0.0f, -1.0f)}, {1, Vector3(1.0f, 0.0f, 0.0f)},  {6, Vector3(-1.0f, 0.0f, 0.0f)},
    };

    Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);

    Quaternion invRot;
    m_rotation.Inverse(invRot);
    Vector3 localUp = Vector3::Transform(worldUp, invRot);

    float maxDot = -100.0f;
    int bestFace = 1;

    for (const auto& face : faces)
    {
        float dot = face.localDir.Dot(localUp);

        if (dot > maxDot)
        {
            maxDot = dot;
            bestFace = face.number;
        }
    }

    return bestFace;
}