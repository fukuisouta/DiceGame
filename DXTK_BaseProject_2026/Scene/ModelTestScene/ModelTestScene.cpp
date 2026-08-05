#include "pch.h"
#include "ModelTestScene.h"
#include "Scene/EditorScene/SceneSerializer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void ModelTestScene::OnEnter(GameContext& gameContext)
{
    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

 
    m_collisionRenderer = std::make_unique<Imase::CollisionRenderer>(device, context, true, true);

    // scene_layout.json から配置データを読み込む
    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
}

void ModelTestScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float elapsedTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    auto keyboardState = Keyboard::Get().GetState();
    static Keyboard::KeyboardStateTracker tracker;
    tracker.Update(keyboardState);

    ImGuiIO& io = ImGui::GetIO();

    for (auto& obj : m_placedObjects)
    {
        if (obj.tag == "Door")
        {
            float distToDoor = Vector3::Distance(m_player.GetPosition(), obj.position);
            constexpr float autoOpenDistance = 2.0f; // 感知距離

            if (distToDoor < autoOpenDistance)
            {
                obj.isOpen = true; // 近づくと自動で開く
            }
            else
            {
                obj.isOpen = false; // 離れると自動で閉じる
            }
        }
    }

       Vector3 prevPos = m_player.GetPosition();

    if (!io.WantCaptureKeyboard)
    {
        m_player.Update(elapsedTime, keyboardState, m_placedObjects);
    }
   m_isColliding = false;

   
    float playerRadius = 0.5f * m_player.GetScale().x * 0.5f; 
    DirectX::BoundingSphere playerSphere(m_player.GetPosition(), playerRadius);

    for (const auto& obj : m_placedObjects)
    {
        
        if (obj.tag == "Door" && obj.isOpen)
        {
            continue;
        }

        
        Vector3 rotRad = Vector3(XMConvertToRadians(obj.rotation.x), XMConvertToRadians(obj.rotation.y),
                                 XMConvertToRadians(obj.rotation.z));
        Quaternion rotation = Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);

        DirectX::BoundingOrientedBox objBox(
            obj.position, XMFLOAT3(obj.scale.x * 0.5f, obj.scale.y * 0.5f, obj.scale.z * 0.5f), rotation);

       
        if (playerSphere.Intersects(objBox))
        {
            m_isColliding = true;
            m_player.SetPosition(prevPos);
            break;
        }
    }

   Vector3 playerPos = m_player.GetPosition();

    Vector3 cameraPos = playerPos + Vector3(0.0f, 6.0f, -8.0f);
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

  
    for (size_t i = 0; i < m_loadedObjects.size(); ++i)
    {
        auto& obj = m_loadedObjects[i];

        Vector3 renderPos = obj.position;

      
        if (i < m_placedObjects.size() && m_placedObjects[i].tag == "Door" && m_placedObjects[i].isOpen)
        {
            renderPos.y += obj.scale.y * 2.0f;
        }

        Matrix world = Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                       Matrix::CreateTranslation(renderPos);

        if (obj.model)
        {
            obj.model->Draw(context, gameContext.commonStates, world, m_view, m_projection);
        }
        else if (obj.primitive)
        {
            obj.primitive->Draw(world, m_view, m_projection, Colors::White);
        }
    }

   
    m_player.Render(context, gameContext.commonStates, m_view, m_projection);

 
    if (m_collisionRenderer)
    {
        constexpr float margin = 0.0f;

       
        float playerRadius = (0.5f * m_player.GetScale().x * 0.5f) + margin;
        DirectX::BoundingSphere playerSphere(m_player.GetPosition(), playerRadius);
        m_collisionRenderer->AddBoundingVolume(playerSphere);

      
        for (const auto& obj : m_placedObjects)
        {
         
            if (obj.tag == "Door" && obj.isOpen)
            {
                continue;
            }

            Vector3 rotRad = Vector3(XMConvertToRadians(obj.rotation.x), XMConvertToRadians(obj.rotation.y),
                                     XMConvertToRadians(obj.rotation.z));
            Quaternion rotation = Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);

            XMFLOAT3 extents =
                XMFLOAT3((obj.scale.x * 0.5f) + margin, (obj.scale.y * 0.5f) + margin, (obj.scale.z * 0.5f) + margin);

            DirectX::BoundingOrientedBox objBox(obj.position, extents, rotation);
            m_collisionRenderer->AddBoundingVolume(objBox);
        }

        m_collisionRenderer->DrawCollision(context, gameContext.commonStates, m_view, m_projection);
    }

   
}

void ModelTestScene::LoadSceneFromJson(GameContext& gameContext, const std::string& filePath)
{
    SceneData loadedData;
    if (!SceneSerializer::LoadFromJson(filePath, loadedData))
    {
        return;
    }

    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    EffectFactory fx(device);
    fx.SetDirectory(L"Resources/Models");

    m_placedObjects.clear();
    m_loadedObjects.clear();

    for (const auto& objData : loadedData.objects)
    {
        PlacedObject obj;
        obj.cmoFileName = objData.modelName;
        obj.tag = objData.tag;
        obj.position = objData.position;
        obj.rotation = objData.rotation;
        obj.scale = objData.scale;

      
        if (objData.tag == "Player")
        {
            m_player.SetPosition(objData.position);
            m_player.SetRotation(objData.rotation);
            m_player.SetScale(objData.scale);

            if (objData.modelName != "Primitive_Cube" && !objData.modelName.empty())
            {
                m_player.SetModelFromData(device, context, "Resources/Models/" + objData.modelName);
            }
            continue;
        }

        m_placedObjects.push_back(obj);

        if (objData.modelName == "Primitive_Cube")
        {
            obj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
            m_loadedObjects.push_back(obj);
        }
        else
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
            }
        }
    }
}