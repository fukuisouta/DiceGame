#include "pch.h"
#include "EditorScene.h"
#include "Framework/SceneSerializer.h"
#include <imgui.h>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void EditorScene::OnEnter(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_field.Initialize(device, context, m_gameLogic.GetBoardWidth(), m_gameLogic.GetBoardHeight(), 2.0f);
    m_gameLogic.Initialize(device, context);

    m_field.SetFloorColor(gameContext.floorColor);
    m_transitionToPlayScene = false;

    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_fxFactory->SetDirectory(L"Resources/Models");

    m_cameraPos = Vector3(0.0f, 22.0f, -20.0f);
    Vector3 dir = Vector3::Zero - m_cameraPos;
    dir.Normalize();
    m_cameraYaw = std::atan2(dir.x, dir.z);
    m_cameraPitch = -std::asin(dir.y);

    LoadSceneFromJson(gameContext, m_currentFilePath);
}

void EditorScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    m_field.Update(dt);
    m_gameLogic.Update(dt);

    if (m_transitionToPlayScene)
    {
        SceneId targetScene = (gameContext.selectedMode == GameMode::Advance) ? SceneId::AdvanceGamePlayScene
                                                                              : SceneId::OriginalGamePlayScene;

        sceneController.RequestSwitch(targetScene);
        return;
    }

    if (!gameContext.droppedFilePath.empty())
    {
        OnFileDropped(gameContext, gameContext.droppedFilePath);
        gameContext.droppedFilePath.clear();
    }

    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        m_cameraYaw += io.MouseDelta.x * m_cameraRotateSpeed;
        m_cameraPitch += io.MouseDelta.y * m_cameraRotateSpeed;
        m_cameraPitch = std::clamp(m_cameraPitch, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);
    }

    Matrix camRot = Matrix::CreateRotationX(m_cameraPitch) * Matrix::CreateRotationY(m_cameraYaw);
    Vector3 forward = Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), camRot);
    Vector3 right = Vector3::TransformNormal(Vector3(1.0f, 0.0f, 0.0f), camRot);
    Vector3 up = Vector3::Up;

    if (!io.WantCaptureKeyboard)
    {
        float speed = m_cameraMoveSpeed * dt;
        if (io.KeyShift)
            speed *= 2.5f;

        auto kb = Keyboard::Get().GetState();
        if (kb.IsKeyDown(Keyboard::Keys::W))
            m_cameraPos += forward * speed;
        if (kb.IsKeyDown(Keyboard::Keys::S))
            m_cameraPos -= forward * speed;
        if (kb.IsKeyDown(Keyboard::Keys::D))
            m_cameraPos += right * speed;
        if (kb.IsKeyDown(Keyboard::Keys::A))
            m_cameraPos -= right * speed;
        if (kb.IsKeyDown(Keyboard::Keys::E))
            m_cameraPos += up * speed;
        if (kb.IsKeyDown(Keyboard::Keys::Q))
            m_cameraPos -= up * speed;
    }

    if (!io.WantCaptureMouse && io.MouseWheel != 0.0f)
    {
        m_cameraPos += forward * (io.MouseWheel * 2.0f);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F))
    {
        m_cameraPos = Vector3(0.0f, 22.0f, -20.0f);
        Vector3 dir = Vector3::Zero - m_cameraPos;
        dir.Normalize();
        m_cameraYaw = std::atan2(dir.x, dir.z);
        m_cameraPitch = -std::asin(dir.y);
    }

    if (m_editorMode == 1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
    {
        RECT rect = gameContext.deviceResources.GetOutputSize();
        Vector2 mousePos(io.MousePos.x, io.MousePos.y);
        Vector2 screenSize(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));

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

    auto& kb = gameContext.keyboardTracker;
    if (kb.lastState.IsKeyDown(DirectX::Keyboard::Keys::E) && kb.IsKeyPressed(DirectX::Keyboard::Keys::Enter))
    {
        SceneId targetScene = (gameContext.selectedMode == GameMode::Advance) ? SceneId::AdvanceGamePlayScene
                                                                              : SceneId::OriginalGamePlayScene;

        sceneController.RequestSwitch(targetScene);
        return;
    }
}

void EditorScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Matrix camRot = Matrix::CreateRotationX(m_cameraPitch) * Matrix::CreateRotationY(m_cameraYaw);
    Vector3 forward = Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), camRot);

    m_view = Matrix::CreateLookAt(m_cameraPos, m_cameraPos + forward, Vector3::Up);
    m_projection = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 1000.0f);

    m_field.Render(context, gameContext.commonStates, m_view, m_projection, -1, -1, -1, -1);
    m_gameLogic.Render(context, gameContext.commonStates, m_view, m_projection);

    Matrix rootWorld = Matrix::CreateRotationX(XMConvertToRadians(m_sceneRootRotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(m_sceneRootRotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(m_sceneRootRotation.z)) *
                       Matrix::CreateTranslation(m_sceneRootPosition);

    context->RSSetState(gameContext.commonStates.CullNone());

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
            for (auto& mesh : obj.model->meshes)
            {
                for (auto& part : mesh->meshParts)
                {
                    if (auto effect = std::dynamic_pointer_cast<IEffectLights>(part->effect))
                    {
                        effect->EnableDefaultLighting();
                    }
                }
            }

            obj.model->Draw(context, gameContext.commonStates, finalWorld, m_view, m_projection);
        }
    }

    context->RSSetState(gameContext.commonStates.CullCounterClockwise());

    // ── ImGui UI ──
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // 1. 最上部 メインメニューバー
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load Title Scene Data"))
            {
                LoadSceneFromJson(gameContext, "Resources/Data/title_scene.json");
            }
            if (ImGui::MenuItem("Load Select Scene Data"))
            {
                LoadSceneFromJson(gameContext, "Resources/Data/select_scene.json");
            }
            if (ImGui::MenuItem("Load Gameplay Scene Data"))
            {
                LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Current File"))
            {
                SaveSceneToJson(m_currentFilePath);
            }
            if (ImGui::MenuItem("Show in Explorer"))
            {
                wchar_t absolutePath[MAX_PATH];
                _wfullpath(absolutePath, L"Resources\\Models", MAX_PATH);
                ShellExecuteW(NULL, L"open", absolutePath, NULL, NULL, SW_SHOWDEFAULT);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Reset Camera Focus [F]"))
            {
                m_cameraPos = Vector3(0.0f, 22.0f, -20.0f);
                Vector3 dir = Vector3::Zero - m_cameraPos;
                dir.Normalize();
                m_cameraYaw = std::atan2(dir.x, dir.z);
                m_cameraPitch = -std::asin(dir.y);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Play"))
        {
            if (ImGui::MenuItem("Return to Game Scene"))
            {
                m_transitionToPlayScene = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImVec2 workPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y);
    ImVec2 workSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y);

    float rightPanelWidth = 320.0f;
    float topToolbarHeight = 38.0f;
    float leftAreaWidth = workSize.x - rightPanelWidth;

    // 2. 上部ツールバー
    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(ImVec2(leftAreaWidth, topToolbarHeight));
    ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##TopToolbar", nullptr, toolbarFlags);
    {
        bool isGridMode = (m_editorMode == 0);
        bool isObjMode = (m_editorMode == 1);

        if (isGridMode)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Grid Mode"))
            m_editorMode = 0;
        if (isGridMode)
            ImGui::PopStyleColor();

        ImGui::SameLine();

        if (isObjMode)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Scene Objects Mode"))
            m_editorMode = 1;
        if (isObjMode)
            ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("| Target: %s", m_currentFilePath.c_str());

        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            SaveSceneToJson(m_currentFilePath);
        }
    }
    ImGui::End();

    // 3. 右側上段: インスペクター (Info)
    float rightPanelHeightTop = workSize.y * 0.45f;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + leftAreaWidth, workPos.y));
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, rightPanelHeightTop));

    ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        if (m_editorMode == 1)
        {
            if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_placedObjects.size()))
            {
                auto& selObj = m_placedObjects[m_selectedIndex];

                if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Name");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", selObj.name.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Position");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Pos", &selObj.position.x, 0.1f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Rotation");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Rot", &selObj.rotation.x, 1.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Scale");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Scale", &selObj.scale.x, 0.05f);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Deselect", ImVec2(-1, 0)))
                    m_selectedIndex = -1;
            }
            else
            {
                ImGui::TextDisabled("No object selected.");
            }
        }
        else // Grid Mode
        {
            ImGui::Text("--- Field & Dice Settings ---");
            static int selectedX = 0, selectedY = 0;
            int maxW = m_gameLogic.GetBoardWidth() - 1;
            int maxH = m_gameLogic.GetBoardHeight() - 1;
            if (selectedX > maxW)
                selectedX = maxW;
            if (selectedY > maxH)
                selectedY = maxH;

            ImGui::SliderInt("Grid X", &selectedX, 0, maxW);
            ImGui::SliderInt("Grid Y", &selectedY, 0, maxH);

            auto* piece = m_gameLogic.GetPieceAt(selectedX, selectedY);
            if (piece)
            {
                ImGui::Text("Cell [%d, %d]: Occupied", selectedX, selectedY);
                int currentOwner = (piece->owner == PlayerType::Player1) ? 0 : 1;
                const char* ownerNames[] = {"Player 1", "Player 2"};
                if (ImGui::Combo("Owner", &currentOwner, ownerNames, 2))
                    piece->owner = (currentOwner == 0) ? PlayerType::Player1 : PlayerType::Player2;

                bool isKing = (piece->type == PieceType::King);
                if (ImGui::Checkbox("Is King", &isKing))
                    piece->type = isKing ? PieceType::King : PieceType::Pawn;

                if (ImGui::Button("Remove Dice", ImVec2(-1, 0)))
                    m_gameLogic.RemovePieceAt(selectedX, selectedY);
            }
            else
            {
                ImGui::Text("Cell [%d, %d]: Empty", selectedX, selectedY);
                static int newOwner = 0;
                const char* ownerNames[] = {"Player 1", "Player 2"};
                ImGui::Combo("New Owner", &newOwner, ownerNames, 2);

                static bool newIsKing = false;
                ImGui::Checkbox("New Is King", &newIsKing);

                static int newTopVal = 1;
                ImGui::SliderInt("New Top Value", &newTopVal, 1, 6);

                if (ImGui::Button("Add Dice", ImVec2(-1, 0)))
                {
                    PlayerType owner = (newOwner == 0) ? PlayerType::Player1 : PlayerType::Player2;
                    PieceType type = newIsKing ? PieceType::King : PieceType::Pawn;
                    m_gameLogic.AddPieceAt(selectedX, selectedY, owner, type, newTopVal);
                }
            }
        }
    }
    ImGui::End();

    // 4. 右側下段: ヒエラルキー & プロジェクト (Scenes)
    float rightPanelHeightBottom = workSize.y - rightPanelHeightTop;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + leftAreaWidth, workPos.y + rightPanelHeightTop));
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, rightPanelHeightBottom));

    ImGui::Begin("Project View", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        if (ImGui::BeginTabBar("ProjectTabs"))
        {
            if (m_editorMode == 1)
            {
                if (ImGui::BeginTabItem("Hierarchy"))
                {
                    if (ImGui::Button("Clear All Objects", ImVec2(-1, 0)))
                    {
                        m_placedObjects.clear();
                        m_selectedIndex = -1;
                    }

                    ImGui::Separator();
                    ImGui::BeginChild("ListRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                    int deleteIndex = -1;
                    for (size_t i = 0; i < m_placedObjects.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        bool isSelected = (static_cast<int>(i) == m_selectedIndex);

                        std::string label = m_placedObjects[i].name;
                        if (ImGui::Selectable(label.c_str(), isSelected))
                        {
                            m_selectedIndex = static_cast<int>(i);
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Delete"))
                                deleteIndex = static_cast<int>(i);
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }

                    if (deleteIndex != -1)
                    {
                        m_placedObjects.erase(m_placedObjects.begin() + deleteIndex);
                        if (m_selectedIndex == deleteIndex)
                            m_selectedIndex = -1;
                        else if (m_selectedIndex > deleteIndex)
                            m_selectedIndex--;
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            else // Grid Mode
            {
                if (ImGui::BeginTabItem("Board Setup"))
                {
                    ImGui::Text("Board Setup");
                    static int newWidth = m_gameLogic.GetBoardWidth();
                    static int newHeight = m_gameLogic.GetBoardHeight();
                    ImGui::SliderInt("Width", &newWidth, 3, 20);
                    ImGui::SliderInt("Height", &newHeight, 3, 20);

                    if (ImGui::Button("Apply Board Size", ImVec2(-1, 0)))
                    {
                        ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
                        m_gameLogic.ResizeBoard(newWidth, newHeight);
                        m_field.Initialize(device, context, newWidth, newHeight, 2.0f);
                        m_field.SetFloorColor(gameContext.floorColor);
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Reset to Default Placement", ImVec2(-1, 0)))
                    {
                        ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
                        m_gameLogic.Initialize(device, context);
                    }
                    ImGui::EndTabItem();
                }
            }

            // Scenes タブ（ダブルクリックでシーンデータ切替読み込み）
            if (ImGui::BeginTabItem("Scenes"))
            {
                ImGui::TextDisabled("Double-click .json to load & edit");
                ImGui::Separator();

                ImGui::BeginChild("SceneListRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                std::string sceneDir = "Resources/Data";
                if (std::filesystem::exists(sceneDir))
                {
                    for (const auto& entry : std::filesystem::directory_iterator(sceneDir))
                    {
                        if (entry.path().extension() == ".json")
                        {
                            std::string filename = entry.path().filename().string();
                            std::string fullPath = entry.path().string();

                            bool isCurrent = (fullPath == m_currentFilePath);
                            if (isCurrent)
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

                            ImGui::Selectable(filename.c_str());

                            if (isCurrent)
                                ImGui::PopStyleColor();

                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                LoadSceneFromJson(gameContext, fullPath);
                            }
                        }
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Folder not found:");
                    ImGui::TextWrapped("%s", sceneDir.c_str());
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
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

    m_currentFilePath = filePath;
    auto device = gameContext.deviceResources.GetD3DDevice();

    m_placedObjects.clear();
    m_selectedIndex = -1;

    for (const auto& objData : loadedData.objects)
    {
        PlacedObject newObj;
        newObj.cmoFileName = objData.modelName;
        newObj.tag = objData.tag;
        newObj.name = objData.modelName + "_" + std::to_string(m_placedObjects.size() + 1);
        newObj.position = objData.position;
        newObj.rotation = objData.rotation;
        newObj.scale = objData.scale;

        std::string fullPathStr = "Resources/Models/" + objData.modelName;
        std::wstring fullPath(fullPathStr.begin(), fullPathStr.end());

        try
        {
            newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), *m_fxFactory);
            m_placedObjects.push_back(newObj);
        }
        catch (...)
        {
        }
    }
}

void EditorScene::SaveSceneToJson(const std::string& filePath)
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
    SceneSerializer::SaveToJson(filePath, saveData);
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

            PlacedObject newObj;
            newObj.cmoFileName = path.filename().string();
            newObj.name = newObj.cmoFileName + "_" + std::to_string(m_placedObjects.size() + 1);
            newObj.tag = "Default";

            newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), *m_fxFactory);
            newObj.position = Vector3(0.0f, 0.0f, 0.0f);
            newObj.rotation = Vector3(0.0f, 0.0f, 0.0f);
            newObj.scale = Vector3(1.0f, 1.0f, 1.0f);

            m_placedObjects.push_back(newObj);
            m_selectedIndex = static_cast<int>(m_placedObjects.size()) - 1;
        }
        catch (...)
        {
        }
    }
}

bool EditorScene::RayIntersectsAABB(const Vector3& rayOrigin, const Vector3& rayDir, const Vector3& boxMin,
                                    const Vector3& boxMax, float& outT)
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