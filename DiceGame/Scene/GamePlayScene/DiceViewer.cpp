#include "pch.h"
#include "DiceViewer.h"
#include <algorithm>

using namespace DirectX::SimpleMath;

void DiceViewer::Update(const DirectX::Mouse::State& mouseState, float dt)
{
    if (!m_isOpen)
        return;

    // --- 左ドラッグ開始 ---
    if (mouseState.leftButton && !m_isDragging)
    {
        m_isDragging = true;
        m_lastMouseX = mouseState.x;
        m_lastMouseY = mouseState.y;
    }
    // --- ドラッグ中（スクリーン基準の回転処理）---
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
    // --- ドラッグ終了 ---
    else if (!mouseState.leftButton)
    {
        m_isDragging = false;
    }

    // --- マウスホイールによる拡大・縮小（ズーム） ---
    int scrollDelta = mouseState.scrollWheelValue - m_lastScrollWheel;
    if (scrollDelta != 0)
    {
        float zoomAmount = static_cast<float>(scrollDelta) * 0.003f;
        m_cameraDistance -= zoomAmount;
        m_cameraDistance = std::clamp(m_cameraDistance, m_minDistance, m_maxDistance);
    }
    m_lastScrollWheel = mouseState.scrollWheelValue;
}

Matrix DiceViewer::GetWorldMatrix() const
{
    return Matrix::CreateFromQuaternion(m_rotation);
}

Matrix DiceViewer::GetViewMatrix() const
{
    Vector3 eye = Vector3(0.0f, 0.0f, -m_cameraDistance);
    Vector3 target = Vector3(0.0f, 0.0f, 0.0f);
    return Matrix::CreateLookAt(eye, target, Vector3::Up);
}

Matrix DiceViewer::GetProjectionMatrix() const
{
    return Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(45.0f), m_aspectRatio, 0.1f, 100.0f);
}