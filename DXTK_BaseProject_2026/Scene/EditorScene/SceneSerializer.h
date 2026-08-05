#pragma once

#include <fstream>
#include <iomanip>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "ObjectData.h"

using json = nlohmann::json;

class SceneSerializer
{
public:
    static bool SaveToJson(const std::string& filePath, const SceneData& sceneData)
    {
        json root;
        root["objects"] = json::array();

        for (const auto& obj : sceneData.objects)
        {
            json objJson;
            objJson["modelName"] = obj.modelName;
            objJson["tag"] = obj.tag; // ★ 追加：タグをJSONに保存
            objJson["position"] = {obj.position.x, obj.position.y, obj.position.z};
            objJson["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
            objJson["scale"] = {obj.scale.x, obj.scale.y, obj.scale.z};

            root["objects"].push_back(objJson);
        }

        // 保存先フォルダが存在しなければ自動作成
        try
        {
            std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        }
        catch (...)
        {
            return false;
        }

        std::ofstream file(filePath);
        if (!file.is_open())
            return false;

        file << std::setw(4) << root << std::endl;
        return true;
    }

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

        if (root.contains("objects") && root["objects"].is_array())
        {
            for (const auto& objJson : root["objects"])
            {
                ObjectTransform obj;
                obj.modelName = objJson.value("modelName", "");
                // ★ 追加：tag が無ければ "Default" を自動設定（これで過去のJSONも壊れません！）
                obj.tag = objJson.value("tag", "Default");

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

                outSceneData.objects.push_back(obj);
            }
        }
        return true;
    }
};