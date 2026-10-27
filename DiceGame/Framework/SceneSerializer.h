#pragma once

#include <fstream>
#include <iomanip>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "ObjectData.h"

using json = nlohmann::json;

// ============================================================================
// シーンデータのJSONシリアライズ（保存）およびデシリアライズ（読み込み）クラス
// ============================================================================
class SceneSerializer
{
public:
    // ------------------------------------------------------------------------
    // SceneData を JSON ファイルとして出力保存します
    // [引数] filePath: 保存先パス, sceneData: 保存対象のシーンデータ
    // [戻り値] 保存成功時 true, 失敗時 false
    // ------------------------------------------------------------------------
    static bool SaveToJson(const std::string& filePath, const SceneData& sceneData)
    {
        json root;
        root["objects"] = json::array();

        // シーン内の全オブジェクト構造体をJSON形式へ変換
        for (const auto& obj : sceneData.objects)
        {
            json objJson;
            objJson["modelName"] = obj.modelName;
            objJson["tag"] = obj.tag;
            objJson["position"] = {obj.position.x, obj.position.y, obj.position.z};
            objJson["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
            objJson["scale"] = {obj.scale.x, obj.scale.y, obj.scale.z};

            // 当たり判定スケールの保存
            objJson["collisionScale"] = {obj.collisionScale.x, obj.collisionScale.y, obj.collisionScale.z};

            // 物理演算（RigidBody）パラメータの保存
            json rbJson;
            rbJson["enabled"] = obj.rigidBody.enabled;
            rbJson["useGravity"] = obj.rigidBody.useGravity;
            rbJson["isStatic"] = obj.rigidBody.isStatic;
            rbJson["mass"] = obj.rigidBody.mass;
            rbJson["damping"] = obj.rigidBody.damping;
            objJson["rigidBody"] = rbJson;

            root["objects"].push_back(objJson);
        }

        // 保存先フォルダが存在しない場合は自動作成
        try
        {
            std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        }
        catch (...)
        {
            return false;
        }

        // ファイルオープンおよびインデント整形して書き出し
        std::ofstream file(filePath);
        if (!file.is_open())
            return false;

        file << std::setw(4) << root << std::endl;
        return true;
    }

    // ------------------------------------------------------------------------
    // JSON ファイルを読み込んで SceneData 構造体へ展開します
    // [引数] filePath: 読み込み元パス, outSceneData: 解析結果を受け取る参照
    // [戻り値] 読み込み成功時 true, 失敗時 false
    // ------------------------------------------------------------------------
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

        // "objects" 配列を展開して ObjectTransform に格納
        if (root.contains("objects") && root["objects"].is_array())
        {
            for (const auto& objJson : root["objects"])
            {
                ObjectTransform obj;
                obj.modelName = objJson.value("modelName", "");
                obj.tag = objJson.value("tag", "Default");

                // トランスフォーム属性の安全な取得 (存在・要素数チェック付き)
                if (objJson.contains("position") && objJson["position"].is_array() && objJson["position"].size() >= 3)
                {
                    obj.position = {objJson["position"][0], objJson["position"][1], objJson["position"][2]};
                }
                if (objJson.contains("rotation") && objJson["rotation"].is_array() && objJson["rotation"].size() >= 3)
                {
                    obj.rotation = {objJson["rotation"][0], objJson["rotation"][1], objJson["rotation"][2]};
                }
                if (objJson.contains("scale") && objJson["scale"].is_array() && objJson["scale"].size() >= 3)
                {
                    obj.scale = {objJson["scale"][0], objJson["scale"][1], objJson["scale"][2]};
                }

                // 衝突判定スケールの取得
                if (objJson.contains("collisionScale") && objJson["collisionScale"].is_array() &&
                    objJson["collisionScale"].size() >= 3)
                {
                    obj.collisionScale = {objJson["collisionScale"][0], objJson["collisionScale"][1],
                                          objJson["collisionScale"][2]};
                }

                // 物理演算（RigidBody）パラメータの取得
                if (objJson.contains("rigidBody") && objJson["rigidBody"].is_object())
                {
                    const auto& rbJson = objJson["rigidBody"];
                    obj.rigidBody.enabled = rbJson.value("enabled", false);
                    obj.rigidBody.useGravity = rbJson.value("useGravity", true);
                    obj.rigidBody.isStatic = rbJson.value("isStatic", false);
                    obj.rigidBody.mass = rbJson.value("mass", 1.0f);
                    obj.rigidBody.damping = rbJson.value("damping", 0.98f);
                }

                outSceneData.objects.push_back(obj);
            }
        }
        return true;
    }
};