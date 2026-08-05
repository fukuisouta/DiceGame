#pragma once

#include <memory>
#include <vector>
#include <string>

#include "ImaseLib/SceneManager.h"
#include "../SceneId.h"
#include "Scene/EditorScene/ObjectData.h"
#include "Game.h"
#include "Scene/ModelTestScene/Player.h"
#include "ImaseLib/CollisionRenderer.h"

class ModelTestScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    ModelTestScene() = default;
    ~ModelTestScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

private:
    Player m_player;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    std::vector<PlacedObject> m_placedObjects;
    std::vector<PlacedObject> m_loadedObjects;

    bool m_isColliding = false;

   
    std::unique_ptr<Imase::CollisionRenderer> m_collisionRenderer;
};