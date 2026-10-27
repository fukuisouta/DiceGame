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

// ============================================================================
// JSONデータからのステージモデル自動ロード・配置・レイアウト描画管理クラス
// ============================================================================
class SceneModelManager
{
public:
    SceneModelManager() = default;
    ~SceneModelManager() = default;

    // Direct3D11デバイスおよびエフェクトファクトリの初期化
    void Initialize(ID3D11Device* device);

    // 指定されたパスのJSONファイルから配置オブジェクト情報を読み込み・3Dモデルの生成を行う
    bool LoadFromJson(ID3D11Device* device, const std::string& filePath);

    // 配置されたすべての3Dモデルを一括描画する
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& projection);

    // --- ゲッター / ユーティリティ ---
    std::vector<PlacedObject>& GetPlacedObjects()
    {
        return m_placedObjects;
    }
    const std::vector<PlacedObject>& GetPlacedObjects() const
    {
        return m_placedObjects;
    }

    // 読み込まれたオブジェクトリストのクリア
    void Clear()
    {
        m_placedObjects.clear();
    }

    // タグ名指定による配置オブジェクトの検索
    const PlacedObject* FindObjectByTag(const std::string& tag) const
    {
        for (const auto& obj : m_placedObjects)
        {
            if (obj.tag == tag)
                return &obj;
        }
        return nullptr;
    }

    // 指定タグのオブジェクトの軸平行境界ボックス（AABB）の算出
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
    std::vector<PlacedObject> m_placedObjects;           // 配置されたオブジェクト群のリスト
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory; // モデル用の共通エフェクトファクトリ
};