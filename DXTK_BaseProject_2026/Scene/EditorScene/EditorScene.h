#pragma once

#include <memory>
#include <vector>
#include <string>

// DirectXTK GeometricPrimitive 描画用
#include <GeometricPrimitive.h>

// ImaseLib 関連
#include "ImaseLib/SceneManager.h"
#include "ImaseLib/DebugCamera.h"
#include "ImaseLib/GridFloor.h"

#include "../SceneId.h"
#include "ObjectData.h"
#include "Game.h"
#include "Scene/ModelTestScene/Player.h"

// ProBuilder風を作成する際のステート管理
enum class CubeCreationState
{
    None,
    DrawingBase,    // 床面でドラッグして幅・奥行き（XZ）を決定
    ExtrudingHeight // マウス移動で高さ（Y）を伸ばす
};

// Imase::SceneBase を継承
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

    // レイとAABB（バウンディングボックス）の交差判定関数
    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

private:
    // 操作用カメラとグリッド床
    std::unique_ptr<Imase::DebugCamera> m_debugCamera;
    std::unique_ptr<Imase::GridFloor> m_gridFloor;

    // プレイヤー
    Player m_player;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    bool m_showViewportGrid = true;

    // 配置されたオブジェクトリスト
    std::vector<PlacedObject> m_placedObjects;

    // 選択中のオブジェクトインデックス（-1 は未選択）
    int m_selectedIndex = -1;

    // ProBuilder風 インタラクティブ作成用変数
    CubeCreationState m_creationState = CubeCreationState::None;
    int m_creatingObjectIndex = -1;
    DirectX::SimpleMath::Vector3 m_startGroundPos;

    // シーン全体の一括操作用パラメータ
    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f};
};