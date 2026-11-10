#pragma once

#include <DirectXMath.h>
#include "SimpleMath.h"
#include <Mouse.h>

// ダイスビューワー定数定義
namespace ViewerConstants
{
    constexpr float DEFAULT_CAMERA_DISTANCE = 5.0f;      // デフォルトカメラ距離
    constexpr float DEFAULT_DRAG_SENSITIVITY = 0.005f;   // マウスドラッグ回転感度
    constexpr float MIN_CAMERA_DISTANCE = 2.0f;          // カメラ最短距離
    constexpr float MAX_CAMERA_DISTANCE = 15.0f;         // カメラ最遠距離
    constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f; // 標準アスペクト比
    constexpr float FOV_ANGLE_DEGREE = 45.0f;            // 画角（度）
    constexpr float NEAR_Z = 0.1f;                       // ニアクリップ
    constexpr float FAR_Z = 100.0f;                      // ファークリップ
    constexpr float WHEEL_ZOOM_SCALE = 0.003f;           // ホイールズーム倍率
}

// 単体ダイスの自由回転・ズーム詳細チェック用モデル・カメラビュー
class DiceViewer
{
public:
    DiceViewer() = default;
    ~DiceViewer() = default;

    // 初期化（アスペクト比の設定・カメラリセット）を行う関数
    // 引数 aspectRatio: アスペクト比
    void Initialize(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        ResetCamera();
    }

    // アスペクト比を再設定する関数
    // 引数 aspectRatio: アスペクト比
    void SetAspectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
    }

    // ビューワーモードを開始表示する関数
    void Open()
    {
        m_isOpen = true;
    }

    // ビューワーモードを終了閉じる関数
    void Close()
    {
        m_isOpen = false;
        m_isDragging = false;
    }

    // ビューワーが開かれているかを取得する関数
    // 戻り値: 開いている場合 true
    bool IsOpen() const
    {
        return m_isOpen;
    }

    // カメラおよび回転姿勢をデフォルトにリセットする関数
    void ResetCamera()
    {
        m_rotation = DirectX::SimpleMath::Quaternion::Identity;
        m_cameraDistance = ViewerConstants::DEFAULT_CAMERA_DISTANCE;
        m_isDragging = false;
    }

    // ダイスの元の向き（回転姿勢）を引き継いでカメラ/回転をリセットする関数
    // 引数 initialRotation: 引き継ぐ姿勢姿態
    void ResetCameraWithRotation(const DirectX::SimpleMath::Quaternion& initialRotation)
    {
        m_rotation = initialRotation;
        m_cameraDistance = ViewerConstants::DEFAULT_CAMERA_DISTANCE;
        m_isDragging = false;
    }

    // ドラッグ回転・ホイールズームの更新を行う関数
    // 引数 mouseState: マウス状態, dt: デルタタイム
    void Update(const DirectX::Mouse::State& mouseState, float dt);

    // ワールド行列を取得する関数
    // 戻り値: ワールド行列
    DirectX::SimpleMath::Matrix GetWorldMatrix() const;

    // ビュー行列を取得する関数
    // 戻り値: ビュー行列
    DirectX::SimpleMath::Matrix GetViewMatrix() const;

    // プロジェクション行列を取得する関数
    // 戻り値: プロジェクション行列
    DirectX::SimpleMath::Matrix GetProjectionMatrix() const;

private:
    bool m_isOpen = false;     // モードオープン表示フラグ
    bool m_isDragging = false; // ドラッグ状態フラグ
    int m_lastMouseX = 0;      // 前回マウスX座標
    int m_lastMouseY = 0;      // 前回マウスY座標
    int m_lastScrollWheel = 0; // 前回ホイール値

    float m_sensitivity = ViewerConstants::DEFAULT_DRAG_SENSITIVITY;   // マウスドラッグ回転感度
    float m_cameraDistance = ViewerConstants::DEFAULT_CAMERA_DISTANCE; // カメラ距離
    float m_minDistance = ViewerConstants::MIN_CAMERA_DISTANCE;        // 最近距離
    float m_maxDistance = ViewerConstants::MAX_CAMERA_DISTANCE;        // 最遠距離
    float m_aspectRatio = ViewerConstants::DEFAULT_ASPECT_RATIO;       // 画面アスペクト比

    DirectX::SimpleMath::Quaternion m_rotation = DirectX::SimpleMath::Quaternion::Identity; // 回転姿勢
};