#include "pch.h"
#include "ModelTestScene.h"
#include "../Framework/SceneSerializer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// シーン開始時に呼び出され、描画環境、カメラ、デバッグ用描画機能、モデルデータなどを初期化します
void ModelTestScene::OnEnter(GameContext& gameContext)
{
    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();
    RECT rect = gameContext.deviceResources.GetOutputSize();

    // デバッグ操作用カメラ、グリッド表示、コライダー描画ヘルパーインスタンスを生成します
    m_debugCamera = std::make_unique<Imase::DebugCamera>(rect.right, rect.bottom);
    m_gridFloor = std::make_unique<Imase::GridFloor>(device, context, &gameContext.commonStates);
    m_collisionRenderer = std::make_unique<Imase::CollisionRenderer>(device, context, true, true);

    // 画面アスペクト比に基づき透視投影（プロジェクション）行列を生成します
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(45.0f), static_cast<float>(rect.right) / static_cast<float>(rect.bottom), 0.1f, 1000.0f);

    // テスト用のダイスオブジェクトに3Dモデルを読み込んで初期化します
    m_dice.Initialize(device, context, "Resources/Models/Dice.cmo");

    // シーンエディターで保存したJSONファイルを読み込み、配置オブジェクトとダイスの初期パラメータを再構築します
    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
}

// 毎フレーム呼び出され、キー入力処理、物理挙動の更新、カメラ位置の計算、シーン遷移制御を行います
void ModelTestScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float elapsedTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    ImGuiIO& io = ImGui::GetIO();

    // キーボードの入力状態を取得して1回押しフラグの更新を行います
    auto keyboardState = Keyboard::Get().GetState();
    m_keyboardTracker.Update(keyboardState);

    // スペースキーが押された際、ダイスに上方への衝撃力を与えて物理的に転がします
    if (m_keyboardTracker.IsKeyPressed(Keyboard::Space))
    {
        m_dice.Roll(Vector3(0.0f, 5.0f, 0.0f));
    }

    // 重力や床での反射、摩擦計算を含むダイスの位置・姿勢の更新を行います
    m_dice.Update(elapsedTime);

    // ImGui操作中を除きデバッグカメラを更新し、カメラ位置をダイス座標へ連動させます
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

    // Eキー入力時、SceneManager経由でEditorSceneへの切り替えリクエストを発行します
    if (m_keyboardTracker.IsKeyPressed(Keyboard::E))
    {
        sceneController.RequestSwitch(SceneId::EditorScene);
    }
}

// 毎フレームの背景、ステージモデル、ダイスモデル、コライダー枠線、デバッグUIの描画順序を制御します
void ModelTestScene::Render(GameContext& gameContext)
{
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    // 空間の基準となる格子状の床面を描画します
    if (m_gridFloor)
    {
        m_gridFloor->Render(context, m_view, m_projection);
    }

    // JSONから読み込んだすべての配置オブジェクト（モデルまたはプリミティブ）を描画します
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

    // テスト用のダイス単体オブジェクトを描画します
    m_dice.Render(context, gameContext.commonStates, m_view, m_projection);

    // 当たり判定視覚化機能が有効な場合、各オブジェクトの有向境界ボックス（OBB）を登録して描画します
    if (m_showCollision && m_collisionRenderer)
    {
        // ステージオブジェクトのスケール、回転、衝突用スケールを結合してバウンディングボックスを生成します
        for (const auto& obj : m_stageObjects)
        {
            Vector3 rotRad = Vector3(XMConvertToRadians(obj.rotation.x), XMConvertToRadians(obj.rotation.y),
                                     XMConvertToRadians(obj.rotation.z));
            Quaternion rotation = Quaternion::CreateFromYawPitchRoll(rotRad.y, rotRad.x, rotRad.z);

            Vector3 finalScale = Vector3(obj.scale.x * obj.collisionScale.x, obj.scale.y * obj.collisionScale.y,
                                         obj.scale.z * obj.collisionScale.z);

            XMFLOAT3 extents = XMFLOAT3(finalScale.x * 0.5f, finalScale.y * 0.5f, finalScale.z * 0.5f);

            DirectX::BoundingOrientedBox objBox(obj.position, extents, rotation);
            m_collisionRenderer->AddBoundingVolume(objBox);
        }

        // ダイスオブジェクトの現在のトランスフォームからバウンディングボックスを生成します
        {
            Vector3 dicePos = m_dice.GetPosition();
            Quaternion diceRot = m_dice.GetRotation();
            Vector3 diceScale = m_dice.GetScale();
            Vector3 diceColScale = m_dice.GetCollisionScale();

            Vector3 finalScale =
                Vector3(diceScale.x * diceColScale.x, diceScale.y * diceColScale.y, diceScale.z * diceColScale.z);

            XMFLOAT3 diceExtents = XMFLOAT3(finalScale.x * 0.5f, finalScale.y * 0.5f, finalScale.z * 0.5f);

            DirectX::BoundingOrientedBox diceBox(dicePos, diceExtents, diceRot);
            m_collisionRenderer->AddBoundingVolume(diceBox);
        }

        // 登録されたすべてのコライダー枠線を一括描画します
        m_collisionRenderer->DrawCollision(context, gameContext.commonStates, m_view, m_projection);
    }

    // 画面上にパラメータ表示やボタン操作を行うImGuiウインドウを構成して描画します
    ImGui::Begin("Model Test Scene");

    Vector3 dicePos = m_dice.GetPosition();
    ImGui::Text("Dice Position: (%.2f, %.2f, %.2f)", dicePos.x, dicePos.y, dicePos.z);

    // ダイスの上面の出目をGetUpwardFaceから取得して表示します
    int currentFace = m_dice.GetUpwardFace();
    ImGui::Text("Current Dice Face: [ %d ]", currentFace);

    if (ImGui::Button("Roll Dice (Space)"))
    {
        m_dice.Roll(Vector3(0.0f, 5.0f, 0.0f));
    }

    ImGui::Checkbox("Show Collision Volumes", &m_showCollision);

    ImGui::End();
}

// 外部JSONファイルを読み込み、エディターで配置されたモデルおよびダイスの初期トランスフォーム情報を適用します
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
        // 読み込んだオブジェクトがダイス識別タグまたはファイル名を持つ場合、m_dice側に初期パラメータを設定します
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

        // ダイス以外の一般的な配置オブジェクト情報を構築します
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

        // プリミティブ立方体の場合と3Dモデルファイルの場合で生成処理を分岐してリストに追加します
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