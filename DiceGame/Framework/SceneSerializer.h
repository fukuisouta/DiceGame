#pragma once

#include <fstream>
#include <iomanip>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "ObjectData.h"

using json = nlohmann::json;

// JSONシリアライズ・デシリアライズ用定数定義
namespace SerializerConstants
{
    constexpr int JSON_INDENT_SPACES = 4;       // JSONファイル出力時の整形用インデントスペース数
    constexpr size_t VECTOR3_ELEMENT_COUNT = 3; // 3次元ベクトル成分数 (X, Y, Z)
}

// シーンデータのJSON形式での保存および読み込みを行う静的クラス
class SceneSerializer
{
public:
    // 構成されたSceneData構造体をJSONファイルとしてディスクに保存する関数
    // 引数 filePath: 保存先のファイルパス, sceneData: 保存対象のシーンデータ
    // 戻り値: 保存成功時 true, 失敗時 false
    static bool SaveToJson(const std::string& filePath, const SceneData& sceneData)
    {
        json root;
        root["objects"] = json::array();

        // シーン内の全オブジェクトをJSON形式に変換
        for (const auto& obj : sceneData.objects)
        {
            json objJson;
            objJson["modelName"] = obj.modelName;
            objJson["tag"] = obj.tag;
            objJson["position"] = {obj.position.x, obj.position.y, obj.position.z};
            objJson["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
            objJson["scale"] = {obj.scale.x, obj.scale.y, obj.scale.z};
            objJson["collisionScale"] = {obj.collisionScale.x, obj.collisionScale.y, obj.collisionScale.z};

            // 物理演算属性のJSON変換
            json rbJson;
            rbJson["enabled"] = obj.rigidBody.enabled;
            rbJson["useGravity"] = obj.rigidBody.useGravity;
            rbJson["isStatic"] = obj.rigidBody.isStatic;
            rbJson["mass"] = obj.rigidBody.mass;
            rbJson["damping"] = obj.rigidBody.damping;
            objJson["rigidBody"] = rbJson;

            root["objects"].push_back(objJson);
        }

        // 出力先ディレクトリが存在しない場合は作成する
        try
        {
            std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        }
        catch (...)
        {
            return false;
        }

        // ファイルを開いてインデント整形して保存
        std::ofstream file(filePath);
        if (!file.is_open())
            return false;

        file << std::setw(SerializerConstants::JSON_INDENT_SPACES) << root << std::endl;
        return true;
    }

    // 指定されたJSONファイルを解析してSceneData構造体へ展開する関数
    // 引数 filePath: 読み込み元のファイルパス, outSceneData: 解析結果を格納する参照
    // 戻り値: 読み込み成功時 true, 失敗時 false
    static bool LoadFromJson(const std::string& filePath, SceneData& outSceneData)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        json root;
        try
        {
            file >> root;
        }
        catch (...)
        {
            return false;
        }

        outSceneData.objects.clear();

        // "objects" 配列を展開して各属性をObjectTransformに復元
        if (root.contains("objects") && root["objects"].is_array())
        {
            for (const auto& objJson : root["objects"])
            {
                ObjectTransform obj;
                obj.modelName = objJson.value("modelName", "");
                obj.tag = objJson.value("tag", "Default");

                // 各トランスフォーム属性の配列・要素数判定付き安全取得
                if (objJson.contains("position") && objJson["position"].is_array() &&
                    objJson["position"].size() >= SerializerConstants::VECTOR3_ELEMENT_COUNT)
                {
                    obj.position = {objJson["position"][0], objJson["position"][1], objJson["position"][2]};
                }
                if (objJson.contains("rotation") && objJson["rotation"].is_array() &&
                    objJson["rotation"].size() >= SerializerConstants::VECTOR3_ELEMENT_COUNT)
                {
                    obj.rotation = {objJson["rotation"][0], objJson["rotation"][1], objJson["rotation"][2]};
                }
                if (objJson.contains("scale") && objJson["scale"].is_array() &&
                    objJson["scale"].size() >= SerializerConstants::VECTOR3_ELEMENT_COUNT)
                {
                    obj.scale = {objJson["scale"][0], objJson["scale"][1], objJson["scale"][2]};
                }
                if (objJson.contains("collisionScale") && objJson["collisionScale"].is_array() &&
                    objJson["collisionScale"].size() >= SerializerConstants::VECTOR3_ELEMENT_COUNT)
                {
                    obj.collisionScale = {objJson["collisionScale"][0], objJson["collisionScale"][1],
                                          objJson["collisionScale"][2]};
                }

                // 物理演算パラメータの復元
                if (objJson.contains("rigidBody") && objJson["rigidBody"].is_object())
                {
                    const auto& rbJson = objJson["rigidBody"];
                    obj.rigidBody.enabled = rbJson.value("enabled", false);
                    obj.rigidBody.useGravity = rbJson.value("useGravity", true);
                    obj.rigidBody.isStatic = rbJson.value("isStatic", false);
                    obj.rigidBody.mass = rbJson.value("mass", PhysicsConstants::DEFAULT_MASS);
                    obj.rigidBody.damping = rbJson.value("damping", PhysicsConstants::DEFAULT_DAMPING);
                }

                outSceneData.objects.push_back(obj);
            }
        }
        return true;
    }
};