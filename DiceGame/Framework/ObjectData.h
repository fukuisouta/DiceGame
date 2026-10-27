#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <GeometricPrimitive.h>
#include <Model.h>

// ============================================================================
// 物理演算用コンポーネント構造体
// オブジェクトの質点、速度、力、重力適用状態などの物理パラメータを保持します
// ============================================================================
struct RigidBodyComponent
{
    bool enabled = false;   // 物理演算の有効/無効フラグ
    bool useGravity = true; // 重力加速度の適用フラグ
    bool isStatic = false;  // 固定オブジェクトフラグ（静的障害物など）

    float mass = 1.0f;     // 質量 (Kg)
    float damping = 0.98f; // 移動速度の空気抵抗・減衰率

    DirectX::SimpleMath::Vector3 velocity = {0.0f, 0.0f, 0.0f}; // 現在の移動速度ベクトル
    DirectX::SimpleMath::Vector3 force = {0.0f, 0.0f, 0.0f};    // 累積外力ベクトル

    // 累積外力ベクトルに力を加算します
    void AddForce(const DirectX::SimpleMath::Vector3& f)
    {
        force += f;
    }
};

// ============================================================================
// シリアライズ（JSONファイルの保存および読み込み）専用の軽量トランスフォーム構造体
// 描画リソースを持たず、座標・回転・スケール・物理設定データのみを保持します
// ============================================================================
struct ObjectTransform
{
    std::string modelName;                                            // 参照するモデル名 / 識別名
    std::string tag = "Default";                                      // 識別タグ（例: "Player", "Enemy", "Stage"）
    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};       // ワールド位置 (X, Y, Z)
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};       // オイラー角回転 (Pitch, Yaw, Roll)
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};          // 描画表示スケール
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f}; // 当たり判定用衝突スケール

    RigidBodyComponent rigidBody; // 物理演算パラメータ
};

// ============================================================================
// シーン全体データをまとめるルートコンテナ構造体
// JSONと1対1で相互変換されるオブジェクトリストを保持します
// ============================================================================
struct SceneData
{
    std::vector<ObjectTransform> objects; // シーン内に存在する全オブジェクトのトランスフォーム配列
};

// ============================================================================
// エディタ上およびゲームシーン上に実体化された配置用オブジェクト構造体
// トランスフォームデータに加え、Direct3D描画用モデルポインタを管理します
// ============================================================================
struct PlacedObject
{
    std::string name;            // エディタ表示用オブジェクト名
    std::string cmoFileName;     // ロード元CMOモデルファイル名
    std::string tag = "Default"; // 識別タグ

    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};       // ワールド位置
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};       // オイラー角回転
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};          // 表示スケール
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f}; // 当たり判定スケール

    std::shared_ptr<DirectX::Model> model;                  // DirectXTK 3Dモデルリソース
    std::shared_ptr<DirectX::GeometricPrimitive> primitive; // 簡易形状（Box, Sphere等）描画リソース

    RigidBodyComponent rigidBody; // 実行用物理パラメータ
};