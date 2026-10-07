#pragma once

#include <vector>
#include <random>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>

struct Particle2D
{
    DirectX::SimpleMath::Vector2 position;
    DirectX::SimpleMath::Vector2 velocity;
    DirectX::SimpleMath::Vector4 color;
    float size;
    float life;
    float maxLife;
};

class ParticleManager
{
public:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // パーティクルの更新
    void Update(float dt);

    // パーティクルの描画（GamePlaySceneのPrimitiveBatchとEffectを借りる）
    void Render(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* primitiveBatch,
                DirectX::BasicEffect* effect);

    // クリックエフェクトの発生
    void SpawnClickParticles(float x, float y, int count = 25);

    // 全パーティクルのクリア
    void Clear();

private:
    std::vector<Particle2D> m_particles;
};