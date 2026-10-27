#pragma once

#include <vector>
#include <random>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <CommonStates.h>

// ============================================================================
// 2Dパーティクル単体のパラメータ構造体
// ============================================================================
struct Particle2D
{
    DirectX::SimpleMath::Vector2 position; // 画面上の2D位置座標
    DirectX::SimpleMath::Vector2 velocity; // 移動速度ベクトル
    DirectX::SimpleMath::Vector4 color;    // 色および不透明度 (RGBA)
    float size;                            // パーティクルのサイズ (ピクセル単位)
    float life;                            // 残り生存時間 (秒)
    float maxLife;                         // 出生時の最大生存時間 (秒)
};

// ============================================================================
// 画面タップ・UIクリック時のエフェクト粒子（2Dパーティクル）生成・描画マネージャー
// 全シーン（タイトル・選択・ゲームプレイ等）で利用可能
// ============================================================================
class ParticleManager
{
public:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // どのシーンからでも単体描画できるようにする初期化処理
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // パーティクルの位置更新・減速・ライフタイム・フェードアウト更新
    void Update(float dt);

    // 【汎用描画】全シーン対応の単体描画処理（正射影行列を自動構築）
    void Render(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                const DirectX::CommonStates& states);

    // 【互換描画】外部の PrimitiveBatch と BasicEffect を共用する描画処理
    void Render(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* primitiveBatch,
                DirectX::BasicEffect* effect);

    // 画面クリック位置への弾けるクリックエフェクト粒子の発生
    void SpawnClickParticles(float x, float y, int count = 25);

    // 管理中の全パーティクルを消去
    void Clear();

private:
    std::vector<Particle2D> m_particles; // アクティブな全パーティクルのリスト

    // 汎用描画用のパイプラインリソース
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
};