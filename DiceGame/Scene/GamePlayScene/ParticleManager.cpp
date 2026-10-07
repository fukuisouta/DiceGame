#include "pch.h"
#include "ParticleManager.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void ParticleManager::Update(float dt)
{
    for (auto it = m_particles.begin(); it != m_particles.end();)
    {
        it->life -= dt;
        if (it->life <= 0.0f)
        {
            it = m_particles.erase(it);
        }
        else
        {
            it->position += it->velocity * dt;
            it->velocity *= 0.92f;                // 空気抵抗・減速
            it->color.w = it->life / it->maxLife; // フェードアウト
            ++it;
        }
    }
}

void ParticleManager::Render(ID3D11DeviceContext* context,
                             DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* primitiveBatch,
                             DirectX::BasicEffect* effect)
{
    if (m_particles.empty())
        return;

    // 呼び出し元で必要なステート（ブレンドなど）が設定済みであることを前提とする
    // InputLayoutは呼び出し元で設定されている、もしくはここで設定する
    effect->Apply(context);
    primitiveBatch->Begin();

    for (const auto& p : m_particles)
    {
        float halfSize = p.size * 0.5f;

        VertexPositionColor vTop(Vector3(p.position.x, p.position.y - halfSize, 0.0f), p.color);
        VertexPositionColor vRight(Vector3(p.position.x + halfSize, p.position.y, 0.0f), p.color);
        VertexPositionColor vBottom(Vector3(p.position.x, p.position.y + halfSize, 0.0f), p.color);
        VertexPositionColor vLeft(Vector3(p.position.x - halfSize, p.position.y, 0.0f), p.color);

        primitiveBatch->DrawTriangle(vTop, vRight, vLeft);
        primitiveBatch->DrawTriangle(vLeft, vRight, vBottom);
    }

    primitiveBatch->End();
}

void ParticleManager::SpawnClickParticles(float x, float y, int count)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);
    std::uniform_real_distribution<float> distSpeed(100.0f, 350.0f);
    std::uniform_real_distribution<float> distSize(6.0f, 14.0f);
    std::uniform_real_distribution<float> distLife(0.3f, 0.6f);

    std::vector<Vector4> colors = {Vector4(1.0f, 0.9f, 0.3f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                   Vector4(1.0f, 0.5f, 0.1f, 1.0f), Vector4(0.4f, 0.9f, 1.0f, 1.0f)};
    std::uniform_int_distribution<int> distColor(0, (int)colors.size() - 1);

    for (int i = 0; i < count; ++i)
    {
        float angle = distAngle(rng);
        float speed = distSpeed(rng);

        Particle2D p;
        p.position = Vector2(x, y);
        p.velocity = Vector2(cosf(angle) * speed, sinf(angle) * speed);
        p.color = colors[distColor(rng)];
        p.size = distSize(rng);
        p.life = distLife(rng);
        p.maxLife = p.life;
        m_particles.push_back(p);
    }
}

void ParticleManager::Clear()
{
    m_particles.clear();
}