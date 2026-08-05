#include "pch.h"
#include "ModelTestScene.h"
#include "Scene/EditorScene/SceneSerializer.h"
#include "imgui.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void ModelTestScene::OnEnter(GameContext& gameContext)
{
    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    m_loadedObjects.clear();
    m_placedObjects.clear();
    m_isColliding = false;

    m_player.SetPosition({0.0f, 1.0f, 0.0f});
    m_player.SetModelFromData(device, context, "Primitive_Cube");

    SceneData loadedData;
    if (SceneSerializer::LoadFromJson("Resources/Data/scene_layout.json", loadedData))
    {
        EffectFactory fx(device);
        fx.SetDirectory(L"Resources/Models");

        for (const auto& objData : loadedData.objects)
        {
            if (objData.tag == "Player")
            {
                m_player.SetPosition(objData.position);
                m_player.SetScale(objData.scale);
                m_player.SetRotation(objData.rotation);
                m_player.SetModelFromData(device, context, objData.modelName);
                continue;
            }

            // Playerに渡す衝突判定用データ
            PlacedObject pObj;
            pObj.position = objData.position;
            pObj.scale = objData.scale;
            pObj.rotation = objData.rotation;
            pObj.tag = objData.tag;
            m_placedObjects.push_back(pObj);

            // 描画用データ
            LoadedObject obj;
            obj.tag = objData.tag;
            obj.position = objData.position;
            obj.rotation = objData.rotation;
            obj.scale = objData.scale;

            if (objData.modelName == "Primitive_Cube")
            {
                obj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
                m_loadedObjects.push_back(obj);
            }
            else if (!objData.modelName.empty())
            {
                std::string fullPathStr = "Resources/Models/" + objData.modelName;
                std::wstring fullPath(fullPathStr.begin(), fullPathStr.end());

                try
                {
                    obj.model = Model::CreateFromCMO(device, fullPath.c_str(), fx);
                    m_loadedObjects.push_back(obj);
                }
                catch (...)
                {
                    obj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
                    m_loadedObjects.push_back(obj);
                }
            }
        }
    }
}

void ModelTestScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float elapsedTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    auto keyboardState = Keyboard::Get().GetState();
    static Keyboard::KeyboardStateTracker tracker;
    tracker.Update(keyboardState);

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard)
    {
        // ★ 第3引数に m_placedObjects を渡すことでビルドエラーを解消
        m_player.Update(elapsedTime, keyboardState, m_placedObjects);
    }

    // ─────────────────────────────────────────────────────────────
    // Scene側でのヒットテスト（球 vs AABB）
    // ─────────────────────────────────────────────────────────────
    m_isColliding = false;

    DirectX::BoundingSphere playerSphere;
    playerSphere.Center = m_player.GetPosition();
    playerSphere.Radius = 0.5f * m_player.GetScale().x;

    for (const auto& obj : m_placedObjects)
    {
        DirectX::BoundingBox objBox;
        objBox.Center = obj.position;
        objBox.Extents = XMFLOAT3(obj.scale.x * 0.5f, obj.scale.y * 0.5f, obj.scale.z * 0.5f);

        if (playerSphere.Intersects(objBox))
        {
            m_isColliding = true;
            break;
        }
    }

    // カメラ更新
    Vector3 playerPos = m_player.GetPosition();
    Vector3 cameraPos = playerPos + Vector3(0.0f, 6.0f, -10.0f);
    Vector3 cameraTarget = playerPos + Vector3(0.0f, 1.0f, 0.0f);

    RECT rect = gameContext.deviceResources.GetOutputSize();
    m_view = Matrix::CreateLookAt(cameraPos, cameraTarget, Vector3::Up);
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(45.0f), static_cast<float>(rect.right) / static_cast<float>(rect.bottom), 0.1f, 1000.0f);

    if (tracker.IsKeyPressed(Keyboard::R) && !io.WantCaptureKeyboard)
    {
        sceneController.RequestSwitch(SceneId::Editor);
    }
}

void ModelTestScene::Render(GameContext& gameContext)
{
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    for (const auto& obj : m_loadedObjects)
    {
        Matrix world = Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                       Matrix::CreateTranslation(obj.position);

        if (obj.model)
        {
            obj.model->Draw(context, gameContext.commonStates, world, m_view, m_projection);
        }
        else if (obj.primitive)
        {
            obj.primitive->Draw(world, m_view, m_projection, DirectX::Colors::White);
        }
    }

    m_player.Render(gameContext.deviceResources.GetD3DDeviceContext(), gameContext.commonStates, m_view, m_projection);

    Vector3 playerPos = m_player.GetPosition();
    gameContext.debugRenderer.DrawText(Vector2(20.0f, 20.0f), DirectX::Colors::Yellow,
                                       L"Player Pos: ({:.2f}, {:.2f}, {:.2f})", playerPos.x, playerPos.y, playerPos.z);

    gameContext.debugRenderer.DrawText(Vector2(20.0f, 50.0f),
                                       m_isColliding ? DirectX::Colors::Red : DirectX::Colors::Green, L"Collision: {}",
                                       m_isColliding ? L"HIT!" : L"SAFE");
}