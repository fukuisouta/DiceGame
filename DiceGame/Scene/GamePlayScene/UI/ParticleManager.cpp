#include "pch.h"
#include "ParticleManager.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 全シーン共通の初期化処理
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

// 毎フレームの全パーティクル物理・寿命計算更新
void ParticleManager::Update(float dt)
{
    for (auto it = m_particles.begin(); it != m_particles.end();)
    {
        it->life -= dt;
        if (it->life <= 0.0f)
        {
            // 寿命が尽きた粒子の破棄
            it = m_particles.erase(it);
        }
        else
        {
            // 位置の計算と空気抵抗（減速）
            it->position += it->velocity * dt;
            it->velocity *= 0.92f;                // 摩擦減速係数
            it->color.w = it->life / it->maxLife; // 生存時間に応じたアルファフェードアウト
            ++it;
        }
    }
}

// 【汎用描画】任意のシーンから画面サイズを渡すだけで単体描画
void ParticleManager::Render(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                             const CommonStates& states)
{
    if (m_particles.empty() || !m_primitiveBatch || !m_effect || !m_inputLayout)
        return;

    // 2Dスクリーン座標用（左上: 0,0）の正射影行列を適用
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

// 2Dポリゴン（ひし形クワッド）によるパーティクルの描画
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

        // 中心位置からひし形（頂点4つ）を構築
        VertexPositionColor vTop(Vector3(p.position.x, p.position.y - halfSize, 0.0f), p.color);
        VertexPositionColor vRight(Vector3(p.position.x + halfSize, p.position.y, 0.0f), p.color);
        VertexPositionColor vBottom(Vector3(p.position.x, p.position.y + halfSize, 0.0f), p.color);
        VertexPositionColor vLeft(Vector3(p.position.x - halfSize, p.position.y, 0.0f), p.color);

        // 2つの三角形でひし形を成形描画
        primitiveBatch->DrawTriangle(vTop, vRight, vLeft);
        primitiveBatch->DrawTriangle(vLeft, vRight, vBottom);
    }

    primitiveBatch->End();
}

// 画面指定座標（x, y）へクリックエフェクト粒子を拡散発生
void ParticleManager::SpawnClickParticles(float x, float y, int count)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);
    std::uniform_real_distribution<float> distSpeed(100.0f, 350.0f);
    std::uniform_real_distribution<float> distSize(6.0f, 14.0f);
    std::uniform_real_distribution<float> distLife(0.3f, 0.6f);

    // 鮮やかな粒子カラー候補（黄、白、オレンジ、水色）
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

// パーティクルの全消去
void ParticleManager::Clear()
{
    m_particles.clear();
}