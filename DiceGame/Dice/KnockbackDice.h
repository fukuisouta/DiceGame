#pragma once

#include "Dice.h"

namespace KnockbackConstants
{
    constexpr float DEFAULT_DURATION = 0.5f;          // 吹っ飛びアニメーションの標準再生時間（秒）
    constexpr float STRAIGHT_SPEED = 25.0f;           // 被弾時に弾き飛ばされる水平移動スピード
    constexpr float DIRECTION_THRESHOLD_SQ = 0.0001f; // 吹っ飛び方向ベクトル計算時のゼロ除算防止用閾値
    constexpr int ANGULAR_VEL_RANGE = 81;             // 吹っ飛び回転角速度のランダム生成範囲 (0～80)
    constexpr int ANGULAR_VEL_OFFSET = 40;            // 吹っ飛び回転角速度のオフセット値 (-40～+40)
    constexpr float SE_VOLUME = 1.0f;                 // 打撃・弾き飛ばし発生時の効果音音量
}

// ============================================================================
// 吹っ飛び演出ダイスクラス
// Base: Dice
// ============================================================================
class KnockbackDice : public Dice
{
public:
    KnockbackDice() = default;
    ~KnockbackDice() override = default;

    // 撃破時の吹っ飛び移動・回転パラメータを設定し演出を開始
    void TriggerKnockback(const DirectX::SimpleMath::Vector3& attackerPos,
                          const DirectX::SimpleMath::Vector3& victimPos,
                          float duration = KnockbackConstants::DEFAULT_DURATION)
    {
        using namespace DirectX::SimpleMath;

        m_position = victimPos;

        // 攻撃者から被弾者への水平直線方向ベクトルを算出
        Vector3 dir = victimPos - attackerPos;
        dir.y = 0.0f;
        if (dir.LengthSquared() > KnockbackConstants::DIRECTION_THRESHOLD_SQ)
        {
            dir.Normalize();
        }
        else
        {
            dir = Vector3(0.0f, 0.0f, 1.0f);
        }

        // 高速直線速度およびランダムな激しい回転角速度を適用
        m_knockbackVelocity = dir * KnockbackConstants::STRAIGHT_SPEED;

        m_knockbackAngularVel = Vector3(
            static_cast<float>(rand() % KnockbackConstants::ANGULAR_VEL_RANGE - KnockbackConstants::ANGULAR_VEL_OFFSET),
            static_cast<float>(rand() % KnockbackConstants::ANGULAR_VEL_RANGE - KnockbackConstants::ANGULAR_VEL_OFFSET),
            static_cast<float>(rand() % KnockbackConstants::ANGULAR_VEL_RANGE -
                               KnockbackConstants::ANGULAR_VEL_OFFSET));

        m_timer = 0.0f;
        m_duration = duration;
        m_isActive = true;

        // 打撃音の再生
        SoundManager::GetInstance().PlaySE(SEType::DiceRoll, KnockbackConstants::SE_VOLUME);
    }

    // 吹っ飛び動作の直線移動、回転、および演出寿命タイマーの更新
    void Update(float dt) override
    {
        if (!m_isActive)
            return;

        using namespace DirectX::SimpleMath;

        m_timer += dt;

        // 直線位置の移動更新
        m_position += m_knockbackVelocity * dt;

        // 回転の更新
        if (m_knockbackAngularVel.LengthSquared() > DiceConstants::ANGULAR_VELOCITY_THRESHOLD)
        {
            Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(
                m_knockbackAngularVel.y * dt, m_knockbackAngularVel.x * dt, m_knockbackAngularVel.z * dt);
            m_rotation = m_rotation * deltaRot;
            m_rotation.Normalize();
        }

        // 演出の終了判定
        if (m_timer >= m_duration)
        {
            m_isActive = false;
        }
    }

    // 吹っ飛び演出が現在アクティブ（実行中）かどうかを取得
    bool IsActive() const
    {
        return m_isActive;
    }

private:
    DirectX::SimpleMath::Vector3 m_knockbackVelocity =
        DirectX::SimpleMath::Vector3::Zero; // 吹っ飛び中の直線平行移動速度ベクトル
    DirectX::SimpleMath::Vector3 m_knockbackAngularVel =
        DirectX::SimpleMath::Vector3::Zero; // 吹っ飛び中の高速回転用角速度ベクトル

    float m_timer = 0.0f;                                    // 吹っ飛び演出開始からの経過時間タイマー（秒）
    float m_duration = KnockbackConstants::DEFAULT_DURATION; // 吹っ飛び演出が持続する合計制限時間（秒）
    bool m_isActive = false;                                 // 吹っ飛び演出が実行中（アクティブ）かどうかのフラグ
};