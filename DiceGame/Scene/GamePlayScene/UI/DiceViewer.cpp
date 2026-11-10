#include "pch.h"
#include "DiceViewer.h"
#include <algorithm>

using namespace DirectX::SimpleMath;

// マウスドラッグ操作による回転およびホイールズーム処理
void DiceViewer::Update(const DirectX::Mouse::State& mouseState, float dt)
{
    (void)dt;
    if (!m_isOpen)
        return;

    if (mouseState.leftButton && !m_isDragging)
    {
        m_isDragging = true;
        m_lastMouseX = mouseState.x;
        m_lastMouseY = mouseState.y;
    }
    else if (mouseState.leftButton && m_isDragging)
    {
        float dx = static_cast<float>(mouseState.x - m_lastMouseX);
        float dy = static_cast<float>(mouseState.y - m_lastMouseY);

        if (dx != 0.0f || dy != 0.0f)
        {
            Quaternion rotX = Quaternion::CreateFromAxisAngle(Vector3::UnitX, -dy * m_sensitivity);
            Quaternion rotY = Quaternion::CreateFromAxisAngle(Vector3::UnitY, dx * m_sensitivity);

            Quaternion deltaRot = rotX * rotY;

            m_rotation = m_rotation * deltaRot;
            m_rotation.Normalize();
        }

        m_lastMouseX = mouseState.x;
        m_lastMouseY = mouseState.y;
    }
    else if (!mouseState.leftButton)
    {
        m_isDragging = false;
    }

    int scrollDelta = mouseState.scrollWheelValue - m_lastScrollWheel;
    if (scrollDelta != 0)
    {
        float zoomAmount = static_cast<float>(scrollDelta) * ViewerConstants::WHEEL_ZOOM_SCALE;
        m_cameraDistance -= zoomAmount;
        m_cameraDistance = std::clamp(m_cameraDistance, m_minDistance, m_maxDistance);
    }
    m_lastScrollWheel = mouseState.scrollWheelValue;
}

// ワールド行列の生成
Matrix DiceViewer::GetWorldMatrix() const
{
    return Matrix::CreateFromQuaternion(m_rotation);
}

// ビュー行列の生成
Matrix DiceViewer::GetViewMatrix() const
{
    Vector3 eye = Vector3(0.0f, 0.0f, -m_cameraDistance);
    Vector3 target = Vector3(0.0f, 0.0f, 0.0f);
    return Matrix::CreateLookAt(eye, target, Vector3::Up);
}

// プロジェクション行列の生成
Matrix DiceViewer::GetProjectionMatrix() const
{
    return Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(ViewerConstants::FOV_ANGLE_DEGREE),
                                                m_aspectRatio, ViewerConstants::NEAR_Z, ViewerConstants::FAR_Z);
}