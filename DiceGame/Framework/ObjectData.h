#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <GeometricPrimitive.h>
#include <Model.h>

// 物理演算に関する共通定数定義
namespace PhysicsConstants
{
    constexpr float DEFAULT_MASS = 1.0f;     // オブジェクトのデフォルト質量 (Kg)
    constexpr float DEFAULT_DAMPING = 0.98f; // 移動速度のデフォルト空気抵抗・減衰率
}

// 物理演算パラメータを保持するコンポーネント構造体
struct RigidBodyComponent
{
    bool enabled = false;   // 物理演算の有効・無効切り替えフラグ
    bool useGravity = true; // 重力加速度を適用するかどうかのフラグ
    bool isStatic = false;  // 位置を固定する静的オブジェクトかどうかのフラグ

    float mass = PhysicsConstants::DEFAULT_MASS;       // オブジェクトの質量 (Kg)
    float damping = PhysicsConstants::DEFAULT_DAMPING; // 移動速度の減衰率 (0.0f～1.0f)

    DirectX::SimpleMath::Vector3 velocity = {0.0f, 0.0f, 0.0f}; // 現在の平行移動速度ベクトル
    DirectX::SimpleMath::Vector3 force = {0.0f, 0.0f, 0.0f};    // フレーム内で加算された累積外力ベクトル

    // 累積外力ベクトルに新たな力を加算する関数
    void AddForce(const DirectX::SimpleMath::Vector3& f)
    {
        force += f;
    }
};

// JSONファイルのシリアライズ・デシリアライズ専用トランスフォーム構造体
struct ObjectTransform
{
    std::string modelName;       // 読み込み対象の3Dモデル名・リソース識別名
    std::string tag = "Default"; // オブジェクトの種別を識別するタグ名 (Player, Enemy, Stage等)
    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};       // ワールド空間上の位置座標 (X, Y, Z)
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};       // オイラー角による回転角度 (Pitch, Yaw, Roll)
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};          // 描画モデルの表示スケール
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f}; // 当たり判定・衝突計算用スケール

    RigidBodyComponent rigidBody; // 物理演算パラメータ構造体
};

// シーン全体データを保持するルートコンテナ構造体
struct SceneData
{
    std::vector<ObjectTransform> objects; // シーン内に存在する全オブジェクトのトランスフォーム配列
};

// ゲーム空間およびエディタ上で実体化された配置用オブジェクト構造体
struct PlacedObject
{
    std::string name;            // エディタのヒエラルキー等で表示するオブジェクト名
    std::string cmoFileName;     // ロード元のCMOモデルファイルパス
    std::string tag = "Default"; // オブジェクトの識別タグ名

    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};       // ワールド空間上の位置座標
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};       // オイラー角による回転角度
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};          // 描画モデルの表示スケール
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f}; // 当たり判定計算用スケール

    std::shared_ptr<DirectX::Model> model;                  // DirectXTKの3Dメッシュモデルリソース
    std::shared_ptr<DirectX::GeometricPrimitive> primitive; // 簡易描画用形状プリミティブ (Box, Sphere等)

    RigidBodyComponent rigidBody; // 実行時に使用する物理演算コンポーネント
};