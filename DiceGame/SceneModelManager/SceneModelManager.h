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

// SceneModelManager定数定義
namespace SceneModelConstants
{
    constexpr float HALF_SCALE_FACTOR = 0.5f; // AABB算出時のスケール半値化係数
}

// JSONデータからのステージモデル自動ロード・配置・レイアウト描画管理クラス
class SceneModelManager
{
public:
    SceneModelManager() = default;
    ~SceneModelManager() = default;

    // Direct3D11デバイスおよびエフェクトファクトリの初期化を行う関数
    // 引数 device: Direct3D11デバイスポインタ
    void Initialize(ID3D11Device* device);

    // 指定されたパスのJSONファイルから配置オブジェクト情報を読み込み・3Dモデルの生成を行う関数
    // 引数 device: Direct3D11デバイスポインタ, filePath: 読み込み用JSONファイルパス
    // 戻り値: ロード成功時 true
    bool LoadFromJson(ID3D11Device* device, const std::string& filePath);

    // 配置されたすべての3Dモデルを一括描画する関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, view: ビュー行列, projection: プロジェクション行列
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, const DirectX::SimpleMath::Matrix& view,
                const DirectX::SimpleMath::Matrix& projection);

    // 配置オブジェクトリストの非Const参照を取得する関数
    // 戻り値: 配置オブジェクト配列への参照
    std::vector<PlacedObject>& GetPlacedObjects()
    {
        return m_placedObjects;
    }

    // 配置オブジェクトリストのConst参照を取得する関数
    // 戻り値: 配置オブジェクト配列へのConst参照
    const std::vector<PlacedObject>& GetPlacedObjects() const
    {
        return m_placedObjects;
    }

    // 読み込まれたオブジェクトリストをクリアリセットする関数
    void Clear()
    {
        m_placedObjects.clear();
    }

    // タグ名指定による配置オブジェクトの検索を行う関数
    // 引数 tag: 検索対象のタグ文字列
    // 戻り値: 一致するPlacedObjectへのConstポインタ (未発見時は nullptr)
    const PlacedObject* FindObjectByTag(const std::string& tag) const
    {
        for (const auto& obj : m_placedObjects)
        {
            if (obj.tag == tag)
                return &obj;
        }
        return nullptr;
    }

    // 指定タグのオブジェクトの軸平行境界ボックス（AABB）を算出する関数
    // 引数 tag: 検索対象のタグ文字列, outMin: 算出領域の最小座標出力参照, outMax: 算出領域の最大座標出力参照
    // 戻り値: 算出成功時 true
    bool GetObjectAABB(const std::string& tag, DirectX::SimpleMath::Vector3& outMin,
                       DirectX::SimpleMath::Vector3& outMax) const
    {
        const auto* obj = FindObjectByTag(tag);
        if (!obj)
            return false;

        DirectX::SimpleMath::Vector3 halfScale = obj->scale * SceneModelConstants::HALF_SCALE_FACTOR;
        outMin = obj->position - halfScale;
        outMax = obj->position + halfScale;
        return true;
    }

private:
    std::vector<PlacedObject> m_placedObjects;           // 配置されたオブジェクト群のリスト
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory; // モデル用の共通エフェクトファクトリ
};