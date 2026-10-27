#include "pch.h"
#include "SceneModelManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using json = nlohmann::json;

// エフェクトファクトリの生成とモデルテクスチャディレクトリのパス設定
void SceneModelManager::Initialize(ID3D11Device* device)
{
    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_fxFactory->SetDirectory(L"Resources/Models");
}

// JSONレイアウトファイルからのステージモデル読み込みとインスタンス生成
bool SceneModelManager::LoadFromJson(ID3D11Device* device, const std::string& filePath)
{
    m_placedObjects.clear();

    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    json j;
    file >> j;

    if (j.contains("objects") && j["objects"].is_array())
    {
        for (const auto& item : j["objects"])
        {
            PlacedObject obj;
            // JSONの modelName や cmoFileName から取得して cmoFileName に設定[cite: 64]
            obj.cmoFileName = item.value("cmoFileName", item.value("modelName", ""));
            obj.name = item.value("name", obj.cmoFileName);
            obj.tag = item.value("tag", "Default");

            // トランスフォーム情報（位置・回転・スケール）のパース
            if (item.contains("position") && item["position"].is_array() && item["position"].size() == 3)
            {
                obj.position = Vector3(item["position"][0], item["position"][1], item["position"][2]);
            }
            if (item.contains("rotation") && item["rotation"].is_array() && item["rotation"].size() == 3)
            {
                obj.rotation = Vector3(item["rotation"][0], item["rotation"][1], item["rotation"][2]);
            }
            if (item.contains("scale") && item["scale"].is_array() && item["scale"].size() == 3)
            {
                obj.scale = Vector3(item["scale"][0], item["scale"][1], item["scale"][2]);
            }

            // CMO形式の3Dモデルデータのロード処理[cite: 64]
            if (!obj.cmoFileName.empty())
            {
                std::wstring wPath(obj.cmoFileName.begin(), obj.cmoFileName.end());
                try
                {
                    obj.model = Model::CreateFromCMO(device, wPath.c_str(), *m_fxFactory);
                }
                catch (...)
                {
                    obj.model = nullptr;
                }
            }

            m_placedObjects.push_back(obj);
        }
    }

    return true;
}

// 配置されているすべての3Dモデルをワールド行列に変換してレンダリング
void SceneModelManager::Render(ID3D11DeviceContext* context, CommonStates& states, const Matrix& view,
                               const Matrix& projection)
{
    for (const auto& obj : m_placedObjects)
    {
        if (obj.model)
        {
            // スケール・回転（Pitch/Yaw/Roll）・平行移動行列の合成
            Matrix world =
                Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) * Matrix::CreateTranslation(obj.position);

            obj.model->Draw(context, states, world, view, projection);
        }
    }
}