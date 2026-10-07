#pragma once

#include <vector>
#include <string>
#include <memory>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include <CommonStates.h>
#include "Framework/ObjectData.h"

class SceneModelManager
{
public:
    SceneModelManager() = default;
    ~SceneModelManager() = default;

    void Initialize(ID3D11Device* device);
    bool LoadFromJson(ID3D11Device* device, const std::string& filePath);
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& projection);

    std::vector<PlacedObject>& GetPlacedObjects()
    {
        return m_placedObjects;
    }
    const std::vector<PlacedObject>& GetPlacedObjects() const
    {
        return m_placedObjects;
    }
    void Clear()
    {
        m_placedObjects.clear();
    }

    // Tag名からオブジェクトを検索
    const PlacedObject* FindObjectByTag(const std::string& tag) const
    {
        for (const auto& obj : m_placedObjects)
        {
            if (obj.tag == tag)
                return &obj;
        }
        return nullptr;
    }

    // AABB（Bounding Box）を取得
    bool GetObjectAABB(const std::string& tag, DirectX::SimpleMath::Vector3& outMin,
                       DirectX::SimpleMath::Vector3& outMax) const
    {
        const auto* obj = FindObjectByTag(tag);
        if (!obj)
            return false;

        DirectX::SimpleMath::Vector3 halfScale = obj->scale * 0.5f;
        outMin = obj->position - halfScale;
        outMax = obj->position + halfScale;
        return true;
    }

private:
    std::vector<PlacedObject> m_placedObjects;
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
};