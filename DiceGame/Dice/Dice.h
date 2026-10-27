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
// ダイス単体の物理挙動（移動・回転）、トランスフォーム管理、モデル描画を行う基本クラス
// ============================================================================
class Dice
{
public:
    Dice() = default;
    virtual ~Dice() = default;

    // Direct3D11 デバイスとモデルパスを受け取り、ダイスを初期化します
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath);

    // --- 位置 (Position) の設定および取得 ---
    void SetPosition(const DirectX::SimpleMath::Vector3& pos)
    {
        m_position = pos;
    }
    DirectX::SimpleMath::Vector3 GetPosition() const
    {
        return m_position;
    }

    // --- 姿勢 (Rotation) の設定および取得 ---
    void SetRotation(const DirectX::SimpleMath::Quaternion& rot)
    {
        m_rotation = rot;
    }
    DirectX::SimpleMath::Quaternion GetRotation() const
    {
        return m_rotation;
    }

    // --- スケール (Scale) の設定および取得 ---
    void SetScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_scale = scale;
    }
    DirectX::SimpleMath::Vector3 GetScale() const
    {
        return m_scale;
    }

    // --- 判定用当たり判定スケールの設定および取得 ---
    void SetCollisionScale(const DirectX::SimpleMath::Vector3& scale)
    {
        m_collisionScale = scale;
    }
    DirectX::SimpleMath::Vector3 GetCollisionScale() const
    {
        return m_collisionScale;
    }

    // 拡大縮小、回転、平行移動から現在のワールド行列（SRT）を算出・取得します
    DirectX::SimpleMath::Matrix GetWorldMatrix() const
    {
        return DirectX::SimpleMath::Matrix::CreateScale(m_scale) *
               DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_rotation) *
               DirectX::SimpleMath::Matrix::CreateTranslation(m_position);
    }

    // ダイスに初速とランダムな角加速度を与えてロール（投擲）を開始します
    void Roll(const DirectX::SimpleMath::Vector3& initialImpulse);

    // ダイスの転がりが完全に停止（着地）したかどうかを返します
    bool IsSettled() const
    {
        return m_isSettled;
    }

    // --- 発光色 (Emissive Color) の設定および取得 ---
    void SetEmissiveColor(const DirectX::SimpleMath::Vector3& color)
    {
        m_emissiveColor = color;
    }
    DirectX::SimpleMath::Vector3 GetEmissiveColor() const
    {
        return m_emissiveColor;
    }

    // 重力適用・床面衝突・回転減衰などの物理演算更新ロジック (毎フレーム呼び出し)
    virtual void Update(float dt);

    // 4引数版 描画処理：内部で自身のワールド行列を生成して描画を実行します
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj) const;

    // 5引数版 描画処理：外部指定された任意のワールド行列を使用して描画を実行します
    virtual void Render(ID3D11DeviceContext* context, DirectX::DX11::CommonStates& states,
                        const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view,
                        const DirectX::SimpleMath::Matrix& proj) const;

    // 現在のダイスの回転姿勢から真上を向いている目（1～6）を取得します
    int GetUpwardFace() const;

    // 任意の回転（クォータニオン）を入力として受け取り、その姿勢における上面の目（1～6）を計算します
    int GetTopFaceFromRotation(const DirectX::SimpleMath::Quaternion& rot) const;

private:
    DirectX::SimpleMath::Vector3 m_position = DirectX::SimpleMath::Vector3::Zero;                   // ワールド座標
    DirectX::SimpleMath::Quaternion m_rotation = DirectX::SimpleMath::Quaternion::Identity;         // 回転姿勢
    DirectX::SimpleMath::Vector3 m_scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);          // 表示スケール
    DirectX::SimpleMath::Vector3 m_collisionScale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f); // 判定スケール

    DirectX::SimpleMath::Vector3 m_velocity = DirectX::SimpleMath::Vector3::Zero;        // 移動速度ベクトル
    DirectX::SimpleMath::Vector3 m_angularVelocity = DirectX::SimpleMath::Vector3::Zero; // 角速度（回転運動）

    bool m_isSettled = false;                                                          // 停止フラグ
    DirectX::SimpleMath::Vector3 m_emissiveColor = DirectX::SimpleMath::Vector3::Zero; // 自発光カラー

    std::unique_ptr<DirectX::Model> m_model; // 3Dモデル参照
};