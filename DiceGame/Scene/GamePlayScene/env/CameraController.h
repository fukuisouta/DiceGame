#pragma once

#include <string>
#include <SimpleMath.h>
#include <Keyboard.h>

// カメラコントローラー用定数定義
namespace CameraControllerConstants
{
    constexpr float DEFAULT_EYE_X = 0.0f;     // カメラ初期位置X
    constexpr float DEFAULT_EYE_Y = 22.0f;    // カメラ初期位置Y
    constexpr float DEFAULT_EYE_Z = -20.0f;   // カメラ初期位置Z
    constexpr float DEFAULT_PITCH = 48.0f;    // デフォルトの見下ろし仰角（度）
    constexpr float DEFAULT_DISTANCE = 30.0f; // デフォルトの注視点からの距離
}

// カメラのパラメーター設定・行列計算およびキー入力操作を行う制御クラス
class CameraController
{
public:
    CameraController() = default;
    ~CameraController() = default;

    // カメラ設定（ピッチ角・距離・ターゲット等）をJSONファイルから読み込む関数
    // 引数 filePath: 読み込み対象のJSONファイルパス
    void LoadConfig(const std::string& filePath);

    // 現在のカメラ設定をJSONファイルへ書き出し保存する関数
    // 引数 filePath: 出力先のJSONファイルパス
    void SaveConfig(const std::string& filePath) const;

    // 毎フレームのキーボード入力による視点操作更新を実行する関数
    // 引数 dt: デルタタイム, kbState: キーボード状態, isCtrlPressed: Ctrlキーが押下されているかのフラグ
    void Update(float dt, const DirectX::Keyboard::State& kbState, bool isCtrlPressed);

    // 算出したビュー行列を取得する関数
    // 戻り値: ビュー変換行列
    DirectX::SimpleMath::Matrix GetViewMatrix() const;

    // 画面のアスペクト比に応じた透視投影（Perspective）プロジェクション行列を算出して取得する関数
    // 引数 aspectRatio: 画面のアスペクト比（幅 / 高さ）
    // 戻り値: プロジェクション変換行列
    DirectX::SimpleMath::Matrix GetProjectionMatrix(float aspectRatio) const;

private:
    DirectX::SimpleMath::Vector3 m_eye = {CameraControllerConstants::DEFAULT_EYE_X,
                                          CameraControllerConstants::DEFAULT_EYE_Y,
                                          CameraControllerConstants::DEFAULT_EYE_Z}; // カメラのワールド空間座標位置

    DirectX::SimpleMath::Vector3 m_target = {0.0f, 0.0f, 0.0f}; // カメラが注視するターゲット座標

    float m_pitch = CameraControllerConstants::DEFAULT_PITCH;       // カメラの見下ろし仰角（度）
    float m_distance = CameraControllerConstants::DEFAULT_DISTANCE; // 注視点からの距離
};