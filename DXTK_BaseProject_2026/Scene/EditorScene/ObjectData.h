#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <GeometricPrimitive.h>
#include <Model.h>


struct ObjectTransform
{
    std::string modelName;
    std::string tag = "Default";
    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};
};

struct SceneData
{
    std::vector<ObjectTransform> objects;
};

struct PlacedObject
{
    std::string name;
    std::string cmoFileName;
    std::string tag = "Default";

    DirectX::SimpleMath::Vector3 position = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 rotation = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 scale = {1.0f, 1.0f, 1.0f};

    std::shared_ptr<DirectX::Model> model;
    std::shared_ptr<DirectX::GeometricPrimitive> primitive;

  
    bool isOpen = false;
};