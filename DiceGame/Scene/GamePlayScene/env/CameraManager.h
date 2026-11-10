#pragma once

#include <DirectXMath.h>
#include <SimpleMath.h>
#include <Keyboard.h>
#include <algorithm>

// カメラマネージャー用定数定義
namespace CameraManagerConstants
{
    // マクロ衝突を避けるため CAM_ などの接頭辞を付与
    constexpr float CAM_DEFAULT_PITCH = 48.0f;    // 標準アングルの見下ろし仰角（度）
    constexpr float CAM_DEFAULT_DISTANCE = 30.0f; // 標準アングルの注視点からの距離
    constexpr float TOP_DOWN_PITCH = 85.0f;       // 真上俯瞰視点の見下ろし仰角（度）
    constexpr float TOP_DOWN_DISTANCE = 25.0f;    // 真上俯瞰視点の注視点からの距離

    constexpr float MIN_DISTANCE = 10.0f;
    constexpr float MAX_DISTANCE = 60.0f;
    constexpr float MIN_PITCH = 15.0f;
    constexpr float MAX_PITCH = 88.0f;

    constexpr float DEBUG_MOVE_SPEED_DISTANCE = 20.0f;
    constexpr float DEBUG_MOVE_SPEED_PITCH = 25.0f;
    constexpr float INTERPOLATION_SPEED = 10.0f;

    constexpr float DEFAULT_EYE_Y = 22.0f;
    constexpr float DEFAULT_EYE_Z = -20.0f;
}

class CameraManager
{
public:
    CameraManager() = default;
    ~CameraManager() = default;

    void Initialize(float defaultPitch = CameraManagerConstants::CAM_DEFAULT_PITCH,
                    float defaultDistance = CameraManagerConstants::CAM_DEFAULT_DISTANCE,
                    const DirectX::SimpleMath::Vector3& target = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f));

    void Update(float dt, const DirectX::Keyboard::State& kbState,
                const class DirectX::Keyboard::KeyboardStateTracker& kbTracker, bool isClient);

    void ToggleView()
    {
        m_isTopDownView = !m_isTopDownView;
        if (m_isTopDownView)
        {
            m_targetPitch = CameraManagerConstants::TOP_DOWN_PITCH;
            m_targetDistance = CameraManagerConstants::TOP_DOWN_DISTANCE;
        }
        else
        {
            m_targetPitch = CameraManagerConstants::CAM_DEFAULT_PITCH;
            m_targetDistance = CameraManagerConstants::CAM_DEFAULT_DISTANCE;
        }
    }

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
    DirectX::SimpleMath::Vector3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_cameraEye{0.0f, CameraManagerConstants::DEFAULT_EYE_Y,
                                             CameraManagerConstants::DEFAULT_EYE_Z};

    float m_cameraPitch = CameraManagerConstants::CAM_DEFAULT_PITCH;
    float m_cameraDistance = CameraManagerConstants::CAM_DEFAULT_DISTANCE;

    bool m_isTopDownView = false;
    float m_targetPitch = CameraManagerConstants::CAM_DEFAULT_PITCH;
    float m_targetDistance = CameraManagerConstants::CAM_DEFAULT_DISTANCE;
};