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

// パーティクルマネージャーデフォルト設定定数
namespace ParticleConstants
{
    constexpr int DEFAULT_CLICK_SPAWN_COUNT = 25; // デフォルトのクリック時粒子生成数
}

// 2Dパーティクル単体のパラメータ構造体
struct Particle2D
{
    DirectX::SimpleMath::Vector2 position; // 画面上の2D位置座標
    DirectX::SimpleMath::Vector2 velocity; // 移動速度ベクトル
    DirectX::SimpleMath::Vector4 color;    // 色および不透明度 (RGBA)
    float size = 0.0f;                     // パーティクルのサイズ (ピクセル単位)
    float life = 0.0f;                     // 残り生存時間 (秒)
    float maxLife = 0.0f;                  // 出生時の最大生存時間 (秒)
};

// 画面タップ・UIクリック時のエフェクト粒子（2Dパーティクル）生成・描画マネージャー
class ParticleManager
{
public:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // パイプラインリソースを初期化する関数
    // 引数 device: Direct3D11デバイス, context: デバイスコンテキスト
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // パーティクルの物理・減速・寿命・不透明度フェードを更新する関数
    // 引数 dt: デルタタイム
    void Update(float dt);

    // 画面解像度から正射影行列を構築して単体描画を行う関数
    // 引数 context: デバイスコンテキスト, screenWidth, screenHeight: 画面解像度, states: コモンステート
    void Render(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                const DirectX::CommonStates& states);

    // 外部の描画バッチとエフェクトを共有して描画を行う関数
    // 引数 context: デバイスコンテキスト, primitiveBatch: カラー描画バッチ, effect: カラーエフェクト
    void Render(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* primitiveBatch,
                DirectX::BasicEffect* effect);

    // 指定の2D画面座標に拡散粒子を発生させる関数
    // 引数 x, y: 発生位置座標, count: 生成粒子数
    void SpawnClickParticles(float x, float y, int count = ParticleConstants::DEFAULT_CLICK_SPAWN_COUNT);

    // 全パーティクルを消去リセットする関数
    void Clear();

private:
    std::vector<Particle2D> m_particles; // アクティブな全パーティクルのリスト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch; // 描画バッチ
    std::unique_ptr<DirectX::BasicEffect> m_effect;                                          // カラーエフェクト
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;                                 // 入力レイアウト
};