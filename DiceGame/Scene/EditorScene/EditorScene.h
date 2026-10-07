#pragma once

#include <memory>
#include <vector>
#include <string>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "../GamePlayScene/GridField.h"
#include "../GamePlayScene/DiceBattleGame.h"
#include "Framework/ObjectData.h"

class EditorScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    EditorScene() = default;
    ~EditorScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    void OnFileDropped(GameContext& gameContext, const std::wstring& fullPath) override;

private:
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
    void SaveSceneToJson(const std::string& filePath);

    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

private:
    GridField m_field;
    DiceBattleGame m_gameLogic;

    int m_editorMode = 0; // 0: Grid Mode, 1: Scene Objects Mode
    bool m_transitionToPlayScene = false;

    // --- 現在編集中のJSONファイルパス ---
    std::string m_currentFilePath = "Resources/Data/scene_layout.json";

    // --- 配置オブジェクトデータ ---
    std::vector<PlacedObject> m_placedObjects;
    int m_selectedIndex = -1;

    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f};

    // カメラパラメータ
    DirectX::SimpleMath::Vector3 m_cameraPos = {0.0f, 22.0f, -20.0f};
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.6f;
    float m_cameraMoveSpeed = 15.0f;
    float m_cameraRotateSpeed = 0.005f;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
};