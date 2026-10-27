#include "pch.h"
#include "CameraManager.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 初期化処理
void CameraManager::Initialize(float defaultPitch, float defaultDistance, const Vector3& target)
{
    m_cameraPitch = defaultPitch;
    m_cameraDistance = defaultDistance;
    m_targetPitch = defaultPitch;
    m_targetDistance = defaultDistance;
    m_cameraTarget = target;
    m_isTopDownView = false;
}

// 毎フレームのカメラ位置更新処理
void CameraManager::Update(float dt, const Keyboard::State& kbState, const Keyboard::KeyboardStateTracker& kbTracker,
                           bool isClient)
{
    bool isCtrlPressed =
        kbState.IsKeyDown(Keyboard::Keys::LeftControl) || kbState.IsKeyDown(Keyboard::Keys::RightControl);

    // デバッグ用の自由カメラ操作（Ctrl + I/K/U/J）
    if (isCtrlPressed)
    {
        float cameraSpeed = 20.0f * dt;
        if (kbState.IsKeyDown(Keyboard::Keys::I))
            m_cameraDistance -= cameraSpeed; // 接近
        if (kbState.IsKeyDown(Keyboard::Keys::K))
            m_cameraDistance += cameraSpeed; // 遠ざかる
        if (kbState.IsKeyDown(Keyboard::Keys::U))
            m_cameraPitch += 25.0f * dt; // 見下ろし角度増加
        if (kbState.IsKeyDown(Keyboard::Keys::J))
            m_cameraPitch -= 25.0f * dt; // 水平に近づく
    }

    // Cキー押下による視点切り替え（通常斜め視点 ⇔ 真上俯瞰視点）
    if (kbTracker.IsKeyPressed(Keyboard::Keys::C))
    {
        ToggleView(); // ★ 視点切り替えを共通関数へ集約
    }

    // デバッグ操作中でない場合、目標値へスムーズに補間移動
    if (!isCtrlPressed)
    {
        m_cameraPitch += (m_targetPitch - m_cameraPitch) * 10.0f * dt;
        m_cameraDistance += (m_targetDistance - m_cameraDistance) * 10.0f * dt;
    }

    // 限界クランプ処理（画角破綻防止）
    m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 60.0f);
    m_cameraPitch = std::clamp(m_cameraPitch, 15.0f, 88.0f);

    float radPitch = XMConvertToRadians(m_cameraPitch);

    // プレイヤーの役割（ホスト 1P / クライアント 2P）に応じてカメラの視点方向を反転決定
    if (isClient)
    {
        // 2P（クライアント）側：奥側から手前を見る方向
        m_cameraEye.y = m_cameraTarget.y + m_cameraDistance * sinf(radPitch);
        m_cameraEye.z = m_cameraTarget.z + m_cameraDistance * cosf(radPitch);
        m_cameraEye.x = m_cameraTarget.x;
    }
    else
    {
        // 1P（ホスト）側：手前側から奥を見る方向
        m_cameraEye.y = m_cameraTarget.y + m_cameraDistance * sinf(radPitch);
        m_cameraEye.z = m_cameraTarget.z - m_cameraDistance * cosf(radPitch);
        m_cameraEye.x = m_cameraTarget.x;
    }
}