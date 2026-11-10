#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include <CommonStates.h>
#include <memory>
#include <string>

#include "Framework/SoundManager.h"

// ============================================================================
// ダイス定数定義
// ============================================================================
namespace DiceConstants
{
    constexpr float DEFAULT_POS_Y = 1.0f;       // 初期スポーン時のY座標（高さ）
    constexpr float FLOOR_Y = 1.0f;             // 床面のY座標境界
    constexpr float GRAVITY = 9.8f;             // 重力加速度 (m/s^2)
    constexpr float BOUNCE_FACTOR = 0.2f;       // 床面バウンド時の反発係数
    constexpr float FRICTION_DAMPING = 0.85f;   // 摩擦による速度・角速度の減衰率
    constexpr float ROLL_IMPULSE_SCALE = 2.5f;  // 投擲インパルスの水平倍率
    constexpr float ROLL_UPWARD_IMPULSE = 5.0f; // 投擲時に加える初速の上昇量

    constexpr float MIN_BOUNCE_SE_SPEED = 1.0f;          // バウンド音を再生する最小落下速度
    constexpr float BOUNCE_STOP_THRESHOLD = 0.2f;        // バウンド処理を完全停止させる速度閾値
    constexpr float ANGULAR_VELOCITY_THRESHOLD = 0.001f; // 回転運動を計算する最小角速度閾値
    constexpr float SETTLE_HEIGHT_MARGIN = 0.05f;        // 着地・停止判定を行う床面からの許容高さ
    constexpr float SETTLE_VELOCITY_THRESHOLD_SQ = 0.2f; // 着地・停止判定を行う速度・角速度の2乗閾値

    constexpr float DOT_PRODUCT_INIT_MIN = -100.0f; // 上面判定用の内積初期最小値

    constexpr int ROLL_ANGULAR_VEL_RANGE = 31;  // ロール時のランダム角速度の生成範囲 (0～30)
    constexpr int ROLL_ANGULAR_VEL_OFFSET = 15; // 角速度のオフセット値 (-15～+15にするため)
}

// ============================================================================
// ダイス単体の物理挙動（移動・回転）、トランスフォーム管理、モデル描画を行う基本クラス
// ============================================================================
class Dice
{
public:
    Dice() = default;
    virtual ~Dice() = default;

    // ダイスモデルのロードおよび初期パラメータのセットアップ
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath);

    // --- トランスフォーム (Position / Rotation / Scale) ---

    // ワールド空間上の位置を設定
    void SetPosition(const DirectX::SimpleMath::Vector3& pos)
    {
        m_position = pos;
    }
    // ワールド空間上の位置を取得
    DirectX::SimpleMath::Vector3 GetPosition() const
    {
        return m_position;
    }

    // ワールド空間上の回転姿勢を設定
    void SetRotation(const DirectX::SimpleMath::Quaternion& rot)
    {
        m_rotation = rot;
    }
    // ワールド空間上の回転姿勢を取得
    DirectX::SimpleMath::Quaternion GetRotation() const
    {
        return m_rotation;
    }

    // 描画用表示スケールを設定
    void SetScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_scale = scale;
    }
    // 描画用表示スケールを取得
    DirectX::SimpleMath::Vector3 GetScale() const
    {
        return m_scale;
    }

    // 当たり判定用スケールを設定
    void SetCollisionScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_collisionScale = scale;
    }
    // 当たり判定用スケールを取得
    DirectX::SimpleMath::Vector3 GetCollisionScale() const
    {
        return m_collisionScale;
    }

    // 現在のスケール・回転・位置からワールド変換行列（SRT）を算出
    DirectX::SimpleMath::Matrix GetWorldMatrix() const
    {
        return DirectX::SimpleMath::Matrix::CreateScale(m_scale) *
               DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_rotation) *
               DirectX::SimpleMath::Matrix::CreateTranslation(m_position);
    }

    // --- ロール・物理運動 ---

    // ダイスに初速とランダムな角速度を与えて物理ロールを開始
    void Roll(const DirectX::SimpleMath::Vector3& initialImpulse);

    // ダイスの物理運動が完全に停止して着地したかを取得
    bool IsSettled() const
    {
        return m_isSettled;
    }

    // --- マテリアル演出 (Emissive) ---

    // 選択時などの自発光カラー（エミッシブ）を設定
    void SetEmissiveColor(const DirectX::SimpleMath::Vector3& color)
    {
        m_emissiveColor = color;
    }
    // 自発光カラー（エミッシブ）を取得
    DirectX::SimpleMath::Vector3 GetEmissiveColor() const
    {
        return m_emissiveColor;
    }

    // 重力・バウンド・減衰などの物理演算および位置姿勢の更新
    virtual void Update(float dt);

    // 自身保持のワールド行列を使用してダイスを描画
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj) const;

    // 外部指定されたワールド行列を使用してダイスを描画
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view,
                        const DirectX::SimpleMath::Matrix& proj) const;

    // 現在の姿勢における上面の出目（1～6）を取得
    int GetUpwardFace() const;

    // 指定された回転姿勢クォータニオンにおける上面の出目（1～6）を計算
    int GetTopFaceFromRotation(const DirectX::SimpleMath::Quaternion& rot) const;

protected:
    DirectX::SimpleMath::Vector3 m_position = DirectX::SimpleMath::Vector3::Zero; // ワールド空間上の現在位置 (X, Y, Z)
    DirectX::SimpleMath::Quaternion m_rotation =
        DirectX::SimpleMath::Quaternion::Identity; // ワールド空間上の現在の回転姿勢（クォータニオン）
    DirectX::SimpleMath::Vector3 m_scale =
        DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f); // 描画用表示スケール (X, Y, Z)
    DirectX::SimpleMath::Vector3 m_collisionScale =
        DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f); // 当たり判定計算用スケール (X, Y, Z)

    DirectX::SimpleMath::Vector3 m_velocity = DirectX::SimpleMath::Vector3::Zero; // 現在の平行移動速度ベクトル
    DirectX::SimpleMath::Vector3 m_angularVelocity =
        DirectX::SimpleMath::Vector3::Zero; // 現在の各軸まわりの回転角速度ベクトル

    bool m_isSettled = false; // 物理運動が完全に停止し着地したかどうかの状態フラグ
    DirectX::SimpleMath::Vector3 m_emissiveColor =
        DirectX::SimpleMath::Vector3::Zero; // 選択・強調表示用の自発光カラー (R, G, B)

private:
    std::unique_ptr<DirectX::Model> m_model; // 描画に使用する3Dモデル（CMO形式）へのユニークポインタ
};