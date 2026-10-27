#pragma once

#include <DirectXMath.h>
#include "SimpleMath.h"
#include <Mouse.h>

// ============================================================================
// 単体ダイスの自由回転・ズーム詳細チェック用モデル・カメラビュー
// ============================================================================
class DiceViewer
{
public:
    DiceViewer() = default;
    ~DiceViewer() = default;

    // 初期化（アスペクト比の設定・カメラリセット）
    void Initialize(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        ResetCamera();
    }

    void SetAspectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
    }

    // モード開始・終了
    void Open()
    {
        m_isOpen = true;
    }

    void Close()
    {
        m_isOpen = false;
        m_isDragging = false;
    }

    bool IsOpen() const
    {
        return m_isOpen;
    }

    // カメラおよび回転姿勢のリセット
    void ResetCamera()
    {
        m_rotation = DirectX::SimpleMath::Quaternion::Identity;
        m_cameraDistance = 5.0f;
        m_isDragging = false;
    }

    // ダイスの元の向き（回転姿勢）を引き継いでカメラ/回転をリセット[cite: 55]
    void ResetCameraWithRotation(const DirectX::SimpleMath::Quaternion& initialRotation)
    {
        m_rotation = initialRotation;
        m_cameraDistance = 5.0f;
        m_isDragging = false;
    }

    // ドラッグ回転・ホイールズームの更新
    void Update(const DirectX::Mouse::State& mouseState, float dt);

    // 行列の取得
    DirectX::SimpleMath::Matrix GetWorldMatrix() const;
    DirectX::SimpleMath::Matrix GetViewMatrix() const;
    DirectX::SimpleMath::Matrix GetProjectionMatrix() const;

private:
    bool m_isOpen = false;
    bool m_isDragging = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    int m_lastScrollWheel = 0;

    float m_sensitivity = 0.005f;  // マウスドラッグ回転感度
    float m_cameraDistance = 5.0f; // カメラ距離
    float m_minDistance = 2.0f;    // 最近距離
    float m_maxDistance = 15.0f;   // 最遠距離
    float m_aspectRatio = 16.0f / 9.0f;

    // クォータニオンによる全方向自由回転保持[cite: 55]
    DirectX::SimpleMath::Quaternion m_rotation = DirectX::SimpleMath::Quaternion::Identity;
};