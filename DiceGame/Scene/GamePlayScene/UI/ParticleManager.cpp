#include "pch.h"
#include "ParticleManager.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// パーティクルスタイル定数定義（マクロ衝突を防ぐため PARTICLE_SIZE_MIN/MAX に変更）
namespace ParticleStyleConstants
{
    constexpr float VELOCITY_FRICTION = 0.92f; // 物理減速摩擦力
    constexpr float SPEED_MIN = 100.0f;        // 粒子射出最小スピード
    constexpr float SPEED_MAX = 350.0f;        // 粒子射出最大スピード
    constexpr float PARTICLE_SIZE_MIN = 6.0f;  // 粒子最小サイズ
    constexpr float PARTICLE_SIZE_MAX = 14.0f; // 粒子最大サイズ
    constexpr float LIFE_MIN = 0.3f;           // 粒子最小寿命 (秒)
    constexpr float LIFE_MAX = 0.6f;           // 粒子最大寿命 (秒)
}

// 初期化
void ParticleManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(false);
    m_effect->SetVertexColorEnabled(true);

    void const* colorByteCode;
    size_t colorByteCodeLength;
    m_effect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);

    device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, colorByteCode,
                              colorByteCodeLength, m_inputLayout.ReleaseAndGetAddressOf());
}

// 物理・寿命更新
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
            it->velocity *= ParticleStyleConstants::VELOCITY_FRICTION;
            it->color.w = it->life / it->maxLife;
            ++it;
        }
    }
}

// 汎用単体描画
void ParticleManager::Render(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                             const CommonStates& states)
{
    if (m_particles.empty() || !m_primitiveBatch || !m_effect || !m_inputLayout)
        return;

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(Matrix::Identity);
    m_effect->SetProjection(orthoProj);

    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.DepthNone(), 0);
    context->RSSetState(states.CullNone());
    context->IASetInputLayout(m_inputLayout.Get());

    Render(context, m_primitiveBatch.get(), m_effect.get());
}

// 描画実行
void ParticleManager::Render(ID3D11DeviceContext* context, PrimitiveBatch<VertexPositionColor>* primitiveBatch,
                             BasicEffect* effect)
{
    if (m_particles.empty() || !primitiveBatch || !effect)
        return;

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

// クリック粒子発生
void ParticleManager::SpawnClickParticles(float x, float y, int count)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);
    std::uniform_real_distribution<float> distSpeed(ParticleStyleConstants::SPEED_MIN,
                                                    ParticleStyleConstants::SPEED_MAX);
    // ★ SIZE_MIN / SIZE_MAX から PARTICLE_SIZE_MIN / PARTICLE_SIZE_MAX へ修正
    std::uniform_real_distribution<float> distSize(ParticleStyleConstants::PARTICLE_SIZE_MIN,
                                                   ParticleStyleConstants::PARTICLE_SIZE_MAX);
    std::uniform_real_distribution<float> distLife(ParticleStyleConstants::LIFE_MIN, ParticleStyleConstants::LIFE_MAX);

    std::vector<Vector4> colors = {Vector4(1.0f, 0.9f, 0.3f, 1.0f), Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                   Vector4(1.0f, 0.5f, 0.1f, 1.0f), Vector4(0.4f, 0.9f, 1.0f, 1.0f)};
    std::uniform_int_distribution<int> distColor(0, static_cast<int>(colors.size()) - 1);

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

// クリア
void ParticleManager::Clear()
{
    m_particles.clear();
}