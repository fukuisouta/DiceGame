#pragma once

#include <vector>
#include <memory>
#include <string>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <Model.h>
#include <GeometricPrimitive.h>
#include "../SceneId.h"
#include "GameContext.h"
#include "ImaseLib/SceneManager.h"
#include "ImaseLib/DebugCamera.h"
#include "ImaseLib/DebugRenderer.h"
#include "Scene/EditorScene/ObjectData.h"
#include "Player.h"

struct LoadedObject
{
    std::shared_ptr<DirectX::Model> model;
    std::shared_ptr<DirectX::GeometricPrimitive> primitive;

    std::string tag;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation;
    DirectX::XMFLOAT3 scale;
};

class ModelTestScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;
    void OnEnter(GameContext& gameContext) override;

private:
    std::vector<LoadedObject> m_loadedObjects;
    std::vector<PlacedObject> m_placedObjects; // Player::Update に渡す用のデータ

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    Player m_player;
    bool m_isColliding = false;
};