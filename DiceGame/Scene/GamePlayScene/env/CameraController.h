#pragma once
#include <SimpleMath.h>
#include <Keyboard.h>

// ============================================================================
// カメラのパラメーター設定・行列計算およびキー入力操作を行う制御クラス
// ============================================================================
class CameraController
{
public:
    CameraController() = default;
    ~CameraController() = default;

    // カメラ設定（ピッチ角・距離・ターゲット等）をJSONファイルから読み込み
    void LoadConfig(const std::string& filePath);
    // 現在のカメラ設定をJSONファイルへ書き出し保存
    void SaveConfig(const std::string& filePath) const;

    // 毎フレームのキーボード入力による視点操作更新
    void Update(float dt, const DirectX::Keyboard::State& kbState, bool isCtrlPressed);

    // ビュー行列の算出・取得
    DirectX::SimpleMath::Matrix GetViewMatrix() const;
    // 画面のアスペクト比に応じた透視投影（Perspective）プロジェクション行列の算出・取得
    DirectX::SimpleMath::Matrix GetProjectionMatrix(float aspectRatio) const;

private:
    DirectX::SimpleMath::Vector3 m_eye = {0.0f, 22.0f, -20.0f}; // カメラのワールド座標位置
    DirectX::SimpleMath::Vector3 m_target = {0.0f, 0.0f, 0.0f}; // 注視点（目標座標）
    float m_pitch = 48.0f;                                      // カメラの見下ろし仰角（度）
    float m_distance = 30.0f;                                   // 注視点からの距離
};