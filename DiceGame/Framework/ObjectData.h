#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <GeometricPrimitive.h>
#include <Model.h>

// 物理演算用パラメータ構造体（データ保持に専念）
struct RigidBodyComponent
{
    bool enabled = false;
    bool useGravity = true;
    bool isStatic = false;

    float mass = 1.0f;
    float damping = 0.98f;

    DirectX::SimpleMath::Vector3 velocity = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 force = {0.0f, 0.0f, 0.0f};

    void AddForce(const DirectX::SimpleMath::Vector3& f)
    {
        force += f;
    }
};

// シリアライズ（JSON保存・読み込み）用の純粋なトランスフォームデータ
struct ObjectTransform
{
    std::string modelName;
    std::string tag = "Default";
    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f};

    RigidBodyComponent rigidBody;
};

// シーン全体のデータコンテナ
struct SceneData
{
    std::vector<ObjectTransform> objects;
};

// エディタやシーン上に実体化された配置オブジェクト
struct PlacedObject
{
    std::string name;
    std::string cmoFileName;
    std::string tag = "Default";

    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};
    DirectX::SimpleMath::Vector3 collisionScale = {1.0f, 1.0f, 1.0f};

    std::shared_ptr<DirectX::Model> model;
    std::shared_ptr<DirectX::GeometricPrimitive> primitive;

    RigidBodyComponent rigidBody;
};