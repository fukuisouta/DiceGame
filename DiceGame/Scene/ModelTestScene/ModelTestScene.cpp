#include "pch.h"
#include "ModelTestScene.h"
#include "../Framework/SceneSerializer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// モデルテストシーンスタイル定数定義
namespace ModelTestStyleConstants
{
    constexpr Vector3 ROLL_IMPULSE_FORCE = Vector3(0.0f, 5.0f, 0.0f); // ダイス転がし時の衝撃力ベクトル
    constexpr float EXTENT_SCALE_FACTOR = 0.5f;                       // バウンディングボックス半幅算出倍率
}

// シーン進入時の初期化処理
void ModelTestScene::OnEnter(GameContext& gameContext)
{
    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();
    RECT rect = gameContext.deviceResources.GetOutputSize();

    m_debugCamera = std::make_unique<Imase::DebugCamera>(rect.right, rect.bottom);
    m_gridFloor = std::make_unique<Imase::GridFloor>(device, context, &gameContext.commonStates);
    m_collisionRenderer = std::make_unique<Imase::CollisionRenderer>(device, context, true, true);

    m_projection =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(ModelTestConstants::DEFAULT_FOV_DEGREE),
                                             static_cast<float>(rect.right) / static_cast<float>(rect.bottom),
                                             ModelTestConstants::NEAR_Z, ModelTestConstants::FAR_Z);

    m_dice.Initialize(device, context, "Resources/Models/Dice.cmo");

    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
}

// 毎フレームの更新処理
void ModelTestScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float elapsedTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    ImGuiIO& io = ImGui::GetIO();

    auto keyboardState = Keyboard::Get().GetState();
    m_keyboardTracker.Update(keyboardState);

    if (m_keyboardTracker.IsKeyPressed(Keyboard::Space))
    {
        m_dice.Roll(ModelTestStyleConstants::ROLL_IMPULSE_FORCE);
    }

    m_dice.Update(elapsedTime);

    if (m_debugCamera)
    {
        if (!io.WantCaptureMouse)
        {
            m_debugCamera->Update(elapsedTime);
        }

        Matrix baseView = m_debugCamera->GetCameraMatrix();
        Vector3 dicePos = m_dice.GetPosition();
        m_view = Matrix::CreateTranslation(-dicePos) * baseView;
    }

    if (m_keyboardTracker.IsKeyPressed(Keyboard::E))
    {
        sceneController.RequestSwitch(SceneId::EditorScene);
    }
}

// 毎フレームの描画処理
void ModelTestScene::Render(GameContext& gameContext)
{
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    if (m_gridFloor)
    {
        m_gridFloor->Render(context, m_view, m_projection);
    }

    for (const auto& obj : m_stageObjects)
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
            obj.primitive->Draw(world, m_view, m_projection, DirectX::Colors::LightGray, nullptr, false,
                                [&]()
                                {
                                    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
                                    context->RSSetState(gameContext.commonStates.CullCounterClockwise());
                                });
        }
    }

    m_dice.Render(context, gameContext.commonStates, m_view, m_projection);

    if (m_showCollision && m_collisionRenderer)
    {
        for (const auto& obj : m_stageObjects)
        {
            Vector3 rotRad = Vector3(XMConvertToRadians(obj.rotation.x), XMConvertToRadians(obj.rotation.y),
                                     XMConvertToRadians(obj.rotation.z));
            Quaternion rotation = Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);

            Vector3 finalScale = Vector3(obj.scale.x * obj.collisionScale.x, obj.scale.y * obj.collisionScale.y,
                                         obj.scale.z * obj.collisionScale.z);

            XMFLOAT3 extents = XMFLOAT3(finalScale.x * ModelTestStyleConstants::EXTENT_SCALE_FACTOR,
                                        finalScale.y * ModelTestStyleConstants::EXTENT_SCALE_FACTOR,
                                        finalScale.z * ModelTestStyleConstants::EXTENT_SCALE_FACTOR);

            DirectX::BoundingOrientedBox objBox(obj.position, extents, rotation);
            m_collisionRenderer->AddBoundingVolume(objBox);
        }

        {
            Vector3 dicePos = m_dice.GetPosition();
            Quaternion diceRot = m_dice.GetRotation();
            Vector3 diceScale = m_dice.GetScale();
            Vector3 diceColScale = m_dice.GetCollisionScale();

            Vector3 finalScale =
                Vector3(diceScale.x * diceColScale.x, diceScale.y * diceColScale.y, diceScale.z * diceColScale.z);

            XMFLOAT3 diceExtents = XMFLOAT3(finalScale.x * ModelTestStyleConstants::EXTENT_SCALE_FACTOR,
                                            finalScale.y * ModelTestStyleConstants::EXTENT_SCALE_FACTOR,
                                            finalScale.z * ModelTestStyleConstants::EXTENT_SCALE_FACTOR);

            DirectX::BoundingOrientedBox diceBox(dicePos, diceExtents, diceRot);
            m_collisionRenderer->AddBoundingVolume(diceBox);
        }

        m_collisionRenderer->DrawCollision(context, gameContext.commonStates, m_view, m_projection);
    }

    ImGui::Begin("Model Test Scene");

    Vector3 dicePos = m_dice.GetPosition();
    ImGui::Text("Dice Position: (%.2f, %.2f, %.2f)", dicePos.x, dicePos.y, dicePos.z);

    int currentFace = m_dice.GetUpwardFace();
    ImGui::Text("Current Dice Face: [ %d ]", currentFace);

    if (ImGui::Button("Roll Dice (Space)"))
    {
        m_dice.Roll(ModelTestStyleConstants::ROLL_IMPULSE_FORCE);
    }

    ImGui::Checkbox("Show Collision Volumes", &m_showCollision);

    ImGui::End();
}

// 外部JSONファイルからのステージレイアウト読み込み
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

    m_stageObjects.clear();

    for (const auto& objData : loadedData.objects)
    {
        if (objData.tag == "Dice" || objData.modelName == "Dice.cmo")
        {
            m_dice.SetPosition(objData.position);

            Vector3 rotRad = Vector3(XMConvertToRadians(objData.rotation.x), XMConvertToRadians(objData.rotation.y),
                                     XMConvertToRadians(objData.rotation.z));
            m_dice.SetRotation(Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z));

            m_dice.SetScale(objData.scale);
            m_dice.SetCollisionScale(objData.collisionScale);
            continue;
        }

        PlacedObject newObj;
        newObj.cmoFileName = objData.modelName;
        newObj.tag = objData.tag;
        newObj.name = (objData.modelName == "Primitive_Cube" ? "ProCube_" : objData.modelName + "_") +
                      std::to_string(m_stageObjects.size() + 1);
        newObj.position = objData.position;
        newObj.rotation = objData.rotation;
        newObj.scale = objData.scale;
        newObj.collisionScale = objData.collisionScale;
        newObj.rigidBody = objData.rigidBody;

        if (objData.modelName == "Primitive_Cube")
        {
            newObj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
            m_stageObjects.push_back(newObj);
        }
        else
        {
            std::string fullPathStr = "Resources/Models/" + objData.modelName;
            std::wstring fullPath(fullPathStr.begin(), fullPathStr.end());

            try
            {
                newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), fx);
                m_stageObjects.push_back(newObj);
            }
            catch (...)
            {
            }
        }
    }
}