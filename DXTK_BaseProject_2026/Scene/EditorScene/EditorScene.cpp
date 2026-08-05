#include "pch.h"
#include "EditorScene.h"
#include "SceneSerializer.h"
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void EditorScene::OnEnter(GameContext& gameContext)
{
    auto device = gameContext.deviceResources.GetD3DDevice();
    auto context = gameContext.deviceResources.GetD3DDeviceContext();
    RECT rect = gameContext.deviceResources.GetOutputSize();

    // デバッグカメラの生成
    m_debugCamera = std::make_unique<Imase::DebugCamera>(rect.right, rect.bottom);

    // グリッド床の生成
    m_gridFloor = std::make_unique<Imase::GridFloor>(device, context, &gameContext.commonStates);

    m_projection = Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(45.0f), static_cast<float>(rect.right) / static_cast<float>(rect.bottom), 0.1f, 1000.0f);

    // デフォルトでJSONからレイアウトをロード
    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
}

void EditorScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float elapsedTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    ImGuiIO& io = ImGui::GetIO();

    // 1. カメラの更新
    if (m_debugCamera && !io.WantCaptureMouse && m_creationState == CubeCreationState::None)
    {
        m_debugCamera->Update(elapsedTime);
        m_view = m_debugCamera->GetCameraMatrix();
    }

    // 2. プレイヤーの更新（入力取得と壁衝突判定を内部で行う）
    auto keyboardState = Keyboard::Get().GetState();
    m_player.Update(elapsedTime, keyboardState, m_placedObjects);

    // --- 画面クリックによるオブジェクト直接選択（レイキャスト） ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse &&
        m_creationState == CubeCreationState::None)
    {
        RECT rect = gameContext.deviceResources.GetOutputSize();
        Vector2 mousePos(io.MousePos.x, io.MousePos.y);
        Vector2 screenSize(static_cast<float>(rect.right), static_cast<float>(rect.bottom));

        float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
        float y = 1.0f - (2.0f * mousePos.y) / screenSize.y;
        Matrix invViewProj = (m_view * m_projection).Invert();

        Vector4 nearPoint = Vector4::Transform(Vector4(x, y, 0.0f, 1.0f), invViewProj);
        Vector4 farPoint = Vector4::Transform(Vector4(x, y, 1.0f, 1.0f), invViewProj);

        Vector3 rayOrigin = Vector3(nearPoint.x / nearPoint.w, nearPoint.y / nearPoint.w, nearPoint.z / nearPoint.w);
        Vector3 rayFar = Vector3(farPoint.x / farPoint.w, farPoint.y / farPoint.w, farPoint.z / farPoint.w);
        Vector3 rayDir = rayFar - rayOrigin;
        rayDir.Normalize();

        float closestT = FLT_MAX;
        int hitIndex = -1;

        for (size_t i = 0; i < m_placedObjects.size(); ++i)
        {
            const auto& obj = m_placedObjects[i];

            // スケールに応じたバウンディングボックスで判定
            Vector3 minBounds = obj.position - (obj.scale * 0.5f);
            Vector3 maxBounds = obj.position + (obj.scale * 0.5f);

            float t = 0.0f;
            if (RayIntersectsAABB(rayOrigin, rayDir, minBounds, maxBounds, t))
            {
                if (t < closestT)
                {
                    closestT = t;
                    hitIndex = static_cast<int>(i);
                }
            }
        }

        m_selectedIndex = hitIndex;
    }

    // --- ProBuilder風 インタラクティブ作成処理 ---
    if (m_creationState != CubeCreationState::None)
    {
        RECT rect = gameContext.deviceResources.GetOutputSize();
        Vector2 mousePos(io.MousePos.x, io.MousePos.y);
        Vector2 screenSize(static_cast<float>(rect.right), static_cast<float>(rect.bottom));

        Vector3 currentGroundPos;
        GetRayGroundIntersection(mousePos, screenSize, currentGroundPos);

        auto& targetObj = m_placedObjects[m_creatingObjectIndex];

        if (m_creationState == CubeCreationState::DrawingBase)
        {
            Vector3 center = (m_startGroundPos + currentGroundPos) * 0.5f;
            Vector3 size = currentGroundPos - m_startGroundPos;

            targetObj.scale.x = std::max(0.1f, std::abs(size.x));
            targetObj.scale.z = std::max(0.1f, std::abs(size.z));
            targetObj.scale.y = 0.2f;

            targetObj.position.x = center.x;
            targetObj.position.z = center.z;
            targetObj.position.y = 0.1f;

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_creationState = CubeCreationState::ExtrudingHeight;
            }
        }
        else if (m_creationState == CubeCreationState::ExtrudingHeight)
        {
            float heightDelta = -io.MouseDelta.y * 0.05f;
            targetObj.scale.y = std::max(0.1f, targetObj.scale.y + heightDelta);
            targetObj.position.y = targetObj.scale.y * 0.5f;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_selectedIndex = m_creatingObjectIndex;
                m_creationState = CubeCreationState::None;
                m_creatingObjectIndex = -1;
            }
        }
    }

    // 3. シーン遷移キー入力
    if (keyboardState.IsKeyDown(Keyboard::E))
    {
        sceneController.RequestSwitch(SceneId::ModelTestScene);
    }
}

void EditorScene::Render(GameContext& gameContext)
{
    auto context = gameContext.deviceResources.GetD3DDeviceContext();

    if (m_showViewportGrid && m_gridFloor)
    {
        m_gridFloor->Render(context, m_view, m_projection);
    }

    // シーンルート（全体）の行列を作成
    Matrix rootWorld = Matrix::CreateRotationX(XMConvertToRadians(m_sceneRootRotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(m_sceneRootRotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(m_sceneRootRotation.z)) *
                       Matrix::CreateTranslation(m_sceneRootPosition);

    for (size_t i = 0; i < m_placedObjects.size(); ++i)
    {
        const auto& obj = m_placedObjects[i];

        Matrix localWorld =
            Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
            Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
            Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) * Matrix::CreateTranslation(obj.position);

        Matrix finalWorld = localWorld * rootWorld;

        if (obj.model)
        {
            obj.model->Draw(context, gameContext.commonStates, finalWorld, m_view, m_projection);
        }
        else if (obj.primitive)
        {
            // 選択中のオブジェクトは黄緑色で強調ハイライト
            XMVECTOR color = (static_cast<int>(i) == m_selectedIndex) ? DirectX::Colors::LimeGreen
                             : (obj.tag == "Player")                  ? DirectX::Colors::SkyBlue
                                                                      : DirectX::Colors::LightGray;

            obj.primitive->Draw(finalWorld, m_view, m_projection, color, nullptr, false,
                                [&]()
                                {
                                    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
                                    context->RSSetState(gameContext.commonStates.CullCounterClockwise());
                                });
        }
    }

    // ★ プレイヤーの描画呼び出し
    m_player.Render(context, gameContext.commonStates, m_view, m_projection);

    ImGui::Begin("Scene Editor Panel");

    // --- 選択中オブジェクト編集用UI ---
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_placedObjects.size()))
    {
        if (ImGui::CollapsingHeader("Selected Object Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& selObj = m_placedObjects[m_selectedIndex];
            ImGui::Text("Selected: %s", selObj.name.c_str());
            ImGui::DragFloat3("Position ##Sel", &selObj.position.x, 0.1f);
            ImGui::DragFloat3("Rotation ##Sel", &selObj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale ##Sel", &selObj.scale.x, 0.05f);

            if (ImGui::Button("Deselect"))
            {
                m_selectedIndex = -1;
            }
        }
        ImGui::Separator();
    }

    // --- シーン全体の一括操作 UI ---
    if (ImGui::CollapsingHeader("Global Scene Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Root Position", &m_sceneRootPosition.x, 0.1f);
        ImGui::DragFloat3("Root Rotation", &m_sceneRootRotation.x, 1.0f);

        if (ImGui::Button("Rotate Y +90 deg"))
        {
            m_sceneRootRotation.y += 90.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rotate Y -90 deg"))
        {
            m_sceneRootRotation.y -= 90.0f;
        }

        if (ImGui::Button("Bake Transform to Objects"))
        {
            Matrix bakeRootWorld = Matrix::CreateRotationX(XMConvertToRadians(m_sceneRootRotation.x)) *
                                   Matrix::CreateRotationY(XMConvertToRadians(m_sceneRootRotation.y)) *
                                   Matrix::CreateRotationZ(XMConvertToRadians(m_sceneRootRotation.z)) *
                                   Matrix::CreateTranslation(m_sceneRootPosition);

            for (auto& obj : m_placedObjects)
            {
                Matrix localWorld = Matrix::CreateScale(obj.scale) *
                                    Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                                    Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                                    Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                                    Matrix::CreateTranslation(obj.position);

                Matrix combined = localWorld * bakeRootWorld;

                Vector3 scale, translation;
                Quaternion rotation;
                combined.Decompose(scale, rotation, translation);

                Vector3 euler = rotation.ToEuler();
                obj.position = translation;
                obj.rotation =
                    Vector3(XMConvertToDegrees(euler.x), XMConvertToDegrees(euler.y), XMConvertToDegrees(euler.z));
                obj.scale = scale;
            }

            m_sceneRootPosition = Vector3::Zero;
            m_sceneRootRotation = Vector3::Zero;
        }
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Viewport Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Viewport Overlay", &m_showViewportGrid);
    }

    if (ImGui::CollapsingHeader("Object Placement", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Draw Box Tool (ProBuilder)"))
        {
            PlacedObject newObj;
            newObj.cmoFileName = "Primitive_Cube";
            newObj.name = "ProCube_" + std::to_string(m_placedObjects.size() + 1);

            newObj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);

            m_placedObjects.push_back(newObj);
            m_creatingObjectIndex = static_cast<int>(m_placedObjects.size() - 1);

            RECT rect = gameContext.deviceResources.GetOutputSize();
            ImGuiIO& io = ImGui::GetIO();
            GetRayGroundIntersection(DirectX::SimpleMath::Vector2(io.MousePos.x, io.MousePos.y),
                                     DirectX::XMFLOAT2(static_cast<float>(rect.right), static_cast<float>(rect.bottom)),
                                     m_startGroundPos);

            m_creationState = CubeCreationState::DrawingBase;
        }

        ImGui::SameLine();

        if (ImGui::Button("Show in Explorer"))
        {
            wchar_t absolutePath[MAX_PATH];
            _wfullpath(absolutePath, L"Resources\\Models", MAX_PATH);
            ShellExecuteW(NULL, L"open", absolutePath, NULL, NULL, SW_SHOWDEFAULT);
        }

        ImGui::TextDisabled("Drag & Drop .cmo / .json files into window.");

        ImGui::Separator();

        if (ImGui::Button("Load Scene from JSON"))
        {
            LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Scene to JSON"))
        {
            SceneData saveData;
            for (const auto& placedObj : m_placedObjects)
            {
                ObjectTransform transform;
                transform.modelName = placedObj.cmoFileName;
                transform.tag = placedObj.tag;
                transform.position = placedObj.position;
                transform.rotation = placedObj.rotation;
                transform.scale = placedObj.scale;

                saveData.objects.push_back(transform);
            }

            SceneSerializer::SaveToJson("Resources/Data/scene_layout.json", saveData);
        }

        if (ImGui::Button("Clear All Objects"))
        {
            m_placedObjects.clear();
            m_selectedIndex = -1;
        }

        ImGui::Separator();

        const char* tagOptions[] = {"Default", "Player", "Wall", "Door"};
        const int tagCount = IM_ARRAYSIZE(tagOptions);

        int deleteIndex = -1;

        // ★ オブジェクトツリー一覧の描画
        for (size_t i = 0; i < m_placedObjects.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));

            std::string label = m_placedObjects[i].name;
            if (m_placedObjects[i].tag == "Player")
                label += " [Player]";

            bool isSelected = (static_cast<int>(i) == m_selectedIndex);

            if (isSelected)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }

            ImGuiTreeNodeFlags flags = isSelected ? ImGuiTreeNodeFlags_Selected : 0;

            if (ImGui::TreeNodeEx(label.c_str(), flags))
            {
                if (ImGui::IsItemClicked())
                {
                    m_selectedIndex = static_cast<int>(i);
                }

                int currentTagIdx = 0;
                for (int t = 0; t < tagCount; ++t)
                {
                    if (m_placedObjects[i].tag == tagOptions[t])
                        currentTagIdx = t;
                }

                if (ImGui::Combo("Tag", &currentTagIdx, tagOptions, tagCount))
                {
                    std::string selectedTag = tagOptions[currentTagIdx];

                    if (selectedTag == "Player")
                    {
                        for (auto& obj : m_placedObjects)
                        {
                            if (obj.tag == "Player")
                                obj.tag = "Default";
                        }

                        // ★ Playerタグに変更されたら m_player に即時データとモデルを反映
                        auto device = gameContext.deviceResources.GetD3DDevice();
                        auto contextDevice = gameContext.deviceResources.GetD3DDeviceContext();

                        m_player.SetPosition(m_placedObjects[i].position);
                        m_player.SetRotation(m_placedObjects[i].rotation);
                        m_player.SetScale(m_placedObjects[i].scale);

                        if (m_placedObjects[i].cmoFileName != "Primitive_Cube" &&
                            !m_placedObjects[i].cmoFileName.empty())
                        {
                            m_player.SetModelFromData(device, contextDevice,
                                                      "Resources/Models/" + m_placedObjects[i].cmoFileName);
                        }
                    }
                    m_placedObjects[i].tag = selectedTag;
                }

                ImGui::DragFloat3("Position", &m_placedObjects[i].position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &m_placedObjects[i].rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &m_placedObjects[i].scale.x, 0.05f);

                if (ImGui::Button("Delete"))
                {
                    deleteIndex = static_cast<int>(i);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (deleteIndex != -1)
        {
            m_placedObjects.erase(m_placedObjects.begin() + deleteIndex);
            if (m_selectedIndex == deleteIndex)
            {
                m_selectedIndex = -1;
            }
        }
    }

    ImGui::End();
}

void EditorScene::LoadSceneFromJson(GameContext& gameContext, const std::string& filePath)
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
    m_selectedIndex = -1;

    for (const auto& objData : loadedData.objects)
    {
        PlacedObject newObj;
        newObj.cmoFileName = objData.modelName;
        newObj.tag = objData.tag;
        newObj.name = (objData.modelName == "Primitive_Cube" ? "ProCube_" : objData.modelName + "_") +
                      std::to_string(m_placedObjects.size() + 1);
        newObj.position = objData.position;
        newObj.rotation = objData.rotation;
        newObj.scale = objData.scale;

        if (objData.modelName == "Primitive_Cube")
        {
            newObj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
            m_placedObjects.push_back(newObj);
        }
        else
        {
            std::string fullPathStr = "Resources/Models/" + objData.modelName;
            std::wstring fullPath(fullPathStr.begin(), fullPathStr.end());

            try
            {
                newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), fx);
                m_placedObjects.push_back(newObj);
            }
            catch (...)
            {
            }
        }

        // ★ "Player" タグのオブジェクトがあれば m_player にトランスフォームとモデルを動的適用
        if (objData.tag == "Player")
        {
            m_player.SetPosition(objData.position);
            m_player.SetRotation(objData.rotation);
            m_player.SetScale(objData.scale);

            if (objData.modelName != "Primitive_Cube" && !objData.modelName.empty())
            {
                m_player.SetModelFromData(device, context, "Resources/Models/" + objData.modelName);
            }
        }
    }
}

void EditorScene::OnFileDropped(GameContext& gameContext, const std::wstring& fullPath)
{
    std::filesystem::path path(fullPath);

    if (path.extension() == L".json")
    {
        LoadSceneFromJson(gameContext, path.string());
        return;
    }

    if (path.extension() == L".cmo")
    {
        try
        {
            auto device = gameContext.deviceResources.GetD3DDevice();

            EffectFactory fx(device);
            fx.SetDirectory(L"Resources/Models");

            PlacedObject newObj;
            newObj.cmoFileName = path.filename().string();
            newObj.name = newObj.cmoFileName + "_" + std::to_string(m_placedObjects.size() + 1);
            newObj.tag = "Default";

            newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), fx);

            newObj.position = Vector3(0.0f, 0.0f, 0.0f);
            newObj.rotation = Vector3(0.0f, 0.0f, 0.0f);
            newObj.scale = Vector3(1.0f, 1.0f, 1.0f);

            m_placedObjects.push_back(newObj);
        }
        catch (...)
        {
        }
    }
}

bool EditorScene::GetRayGroundIntersection(const DirectX::SimpleMath::Vector2& mousePos,
                                           const DirectX::XMFLOAT2& screenSize,
                                           DirectX::SimpleMath::Vector3& outIntersection)
{
    using namespace DirectX::SimpleMath;

    float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / screenSize.y;

    Matrix invViewProj = (m_view * m_projection).Invert();

    Vector4 nearPoint = Vector4::Transform(Vector4(x, y, 0.0f, 1.0f), invViewProj);
    Vector4 farPoint = Vector4::Transform(Vector4(x, y, 1.0f, 1.0f), invViewProj);

    Vector3 rayOrigin = Vector3(nearPoint.x / nearPoint.w, nearPoint.y / nearPoint.w, nearPoint.z / nearPoint.w);
    Vector3 rayFar = Vector3(farPoint.x / farPoint.w, farPoint.y / farPoint.w, farPoint.z / farPoint.w);
    Vector3 rayDir = rayFar - rayOrigin;
    rayDir.Normalize();

    if (std::abs(rayDir.y) < 0.0001f)
        return false;

    float t = -rayOrigin.y / rayDir.y;
    if (t < 0)
        return false;

    outIntersection = rayOrigin + rayDir * t;
    return true;
}

bool EditorScene::RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin,
                                    const DirectX::SimpleMath::Vector3& rayDir,
                                    const DirectX::SimpleMath::Vector3& boxMin,
                                    const DirectX::SimpleMath::Vector3& boxMax, float& outT)
{
    float tmin = (boxMin.x - rayOrigin.x) / (std::abs(rayDir.x) < 0.0001f ? 0.0001f : rayDir.x);
    float tmax = (boxMax.x - rayOrigin.x) / (std::abs(rayDir.x) < 0.0001f ? 0.0001f : rayDir.x);

    if (tmin > tmax)
        std::swap(tmin, tmax);

    float tymin = (boxMin.y - rayOrigin.y) / (std::abs(rayDir.y) < 0.0001f ? 0.0001f : rayDir.y);
    float tymax = (boxMax.y - rayOrigin.y) / (std::abs(rayDir.y) < 0.0001f ? 0.0001f : rayDir.y);

    if (tymin > tymax)
        std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;
    if (tymax < tmax)
        tmax = tymax;

    float tzmin = (boxMin.z - rayOrigin.z) / (std::abs(rayDir.z) < 0.0001f ? 0.0001f : rayDir.z);
    float tzmax = (boxMax.z - rayOrigin.z) / (std::abs(rayDir.z) < 0.0001f ? 0.0001f : rayDir.z);

    if (tzmin > tzmax)
        std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    outT = tmin;
    return outT >= 0.0f;
}

void ResolveWallCollision(DirectX::SimpleMath::Vector3& position, float radius,
                          const std::vector<PlacedObject>& objects)
{
    for (const auto& obj : objects)
    {
        if (obj.tag != "Wall")
            continue;

        Vector3 minBounds = obj.position - (obj.scale * 0.5f);
        Vector3 maxBounds = obj.position + (obj.scale * 0.5f);

        Vector3 closestPoint;
        closestPoint.x = std::max(minBounds.x, std::min(position.x, maxBounds.x));
        closestPoint.y = std::max(minBounds.y, std::min(position.y, maxBounds.y));
        closestPoint.z = std::max(minBounds.z, std::min(position.z, maxBounds.z));

        Vector3 diff = position - closestPoint;
        float distSq = diff.LengthSquared();

        if (distSq < radius * radius && distSq > 0.0001f)
        {
            float dist = std::sqrt(distSq);
            Vector3 normal = diff / dist;
            float penetration = radius - dist;

            position += normal * penetration;
        }
    }
}