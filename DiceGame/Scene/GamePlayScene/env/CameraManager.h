#pragma once

#include <DirectXMath.h>
#include <SimpleMath.h>
#include <Keyboard.h>
#include <algorithm>

// ============================================================================
// ゲームプレイ中のカメラ制御マネージャー
// 通常斜め視点と真上俯瞰視点の補間切り替え、1P/2Pごとの反転視点を管理します
// ============================================================================
class CameraManager
{
public:
    CameraManager() = default;
    ~CameraManager() = default;

    // 初期化（基本パラメータおよびターゲット座標の設定）
    void Initialize(float defaultPitch = 48.0f, float defaultDistance = 30.0f,
                    const DirectX::SimpleMath::Vector3& target = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f));

    // 毎フレームのカメラ位置移動・視点補間および操作更新
    void Update(float dt, const DirectX::Keyboard::State& kbState,
                const class DirectX::Keyboard::KeyboardStateTracker& kbTracker, bool isClient);

    // ★ 視点切り替え処理（通常斜め視点 ⇔ 真上俯瞰視点）
    void ToggleView()
    {
        m_isTopDownView = !m_isTopDownView;
        if (m_isTopDownView)
        {
            m_targetPitch = 85.0f;    // ほぼ真上の角度
            m_targetDistance = 25.0f; // 盤面全体が収まる距離
        }
        else
        {
            m_targetPitch = 48.0f; // 通常の斜めアングル
            m_targetDistance = 30.0f;
        }
    }

    // --- ゲッター・セッター ---
    DirectX::SimpleMath::Vector3 GetEye() const
    {
        return m_cameraEye;
    }
    DirectX::SimpleMath::Vector3 GetTarget() const
    {
        return m_cameraTarget;
    }
    float GetPitch() const
    {
        return m_cameraPitch;
    }
    float GetDistance() const
    {
        return m_cameraDistance;
    }

    void SetPitch(float pitch)
    {
        m_cameraPitch = pitch;
        m_targetPitch = pitch;
    }
    void SetDistance(float dist)
    {
        m_cameraDistance = dist;
        m_targetDistance = dist;
    }
    void SetTarget(const DirectX::SimpleMath::Vector3& target)
    {
        m_cameraTarget = target;
    }

    bool IsTopDownView() const
    {
        return m_isTopDownView;
    }

private:
    DirectX::SimpleMath::Vector3 m_cameraTarget{0.0f, 0.0f, 0.0f}; // カメラ注視点
    DirectX::SimpleMath::Vector3 m_cameraEye{0.0f, 22.0f, -20.0f}; // カメラ視点位置

    float m_cameraPitch = 48.0f;    // 現在の仰角
    float m_cameraDistance = 30.0f; // 現在の距離

    // 視点切り替え機能（Cキー / UIボタン用）
    bool m_isTopDownView = false;   // 真上俯瞰表示フラグ
    float m_targetPitch = 48.0f;    // 補間目標仰角
    float m_targetDistance = 30.0f; // 補間目標距離
};