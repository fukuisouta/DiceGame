#pragma once

#include <memory>
#include <vector>
#include <string>


#include <GeometricPrimitive.h>

#include "ImaseLib/SceneManager.h"
#include "ImaseLib/DebugCamera.h"
#include "ImaseLib/GridFloor.h"

#include "../SceneId.h"
#include "ObjectData.h"
#include "Game.h"
#include "Scene/ModelTestScene/Player.h"
#include "ImaseLib/CollisionRenderer.h"


enum class CubeCreationState
{
    None,
    DrawingBase,   
    ExtrudingHeight 
};


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
    bool GetRayGroundIntersection(const DirectX::SimpleMath::Vector2& mousePos, const DirectX::XMFLOAT2& screenSize,
                                  DirectX::SimpleMath::Vector3& outIntersection);

   
    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

private:
    
    std::unique_ptr<Imase::DebugCamera> m_debugCamera;
    std::unique_ptr<Imase::GridFloor> m_gridFloor;

  
    Player m_player;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    bool m_showViewportGrid = true;
    bool m_showCollision = true; 

    
    std::vector<PlacedObject> m_placedObjects;

    int m_selectedIndex = -1;

    CubeCreationState m_creationState = CubeCreationState::None;
    int m_creatingObjectIndex = -1;
    DirectX::SimpleMath::Vector3 m_startGroundPos;

 
    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f};

    std::unique_ptr<Imase::CollisionRenderer> m_collisionRenderer;
};