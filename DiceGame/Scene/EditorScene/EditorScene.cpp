#include "pch.h"
#include "EditorScene.h"
#include "Framework/SceneSerializer.h"
#include <imgui.h>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <Mouse.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// エディタ表示スタイル定数定義
namespace EditorStyleConstants
{
    constexpr float RIGHT_PANEL_WIDTH = 320.0f;    // 右側パラメータドッキングパネルの横幅
    constexpr float TOP_TOOLBAR_HEIGHT = 38.0f;    // 上部モード切替ツールバーの高さ
    constexpr float RIGHT_PANEL_TOP_RATIO = 0.45f; // 右上Transform表示パネルの高さ割合

    constexpr float DEFAULT_2D_SCALE_X = 3.0f; // 画像ドロップ時の2Dオブジェクト初期Xスケール
    constexpr float DEFAULT_2D_SCALE_Y = 1.5f; // 画像ドロップ時の2Dオブジェクト初期Yスケール
    constexpr float DEFAULT_2D_SCALE_Z = 0.2f; // 画像ドロップ時の2Dオブジェクト初期Zスケール
    constexpr float SKY_MODEL_SCALE = 100.0f;  // スカイモデルドロップ時の初期スケール

    constexpr float CAM_RESET_2D_POS_Z = -10.0f; // 2Dモード時のカメラリセットZ位置
    constexpr float CAM_RESET_3D_POS_Y = 22.0f;  // 3Dモード時のカメラリセットY位置
    constexpr float CAM_RESET_3D_POS_Z = -20.0f; // 3Dモード時のカメラリセットZ位置

    constexpr float DRAG_POS_SPEED = 0.1f;    // ImGuiドラッグ操作による位置変更感度
    constexpr float DRAG_ROT_SPEED = 1.0f;    // ImGuiドラッグ操作による回転変更感度
    constexpr float DRAG_SCALE_SPEED = 0.05f; // ImGuiドラッグ操作によるスケール変更感度

    constexpr float NDC_CENTER = 1.0f;          // 正規化デバイス座標変換係数
    constexpr float SCREEN_CENTER_RATIO = 0.5f; // スクリーン中心倍率
}

namespace
{
    // 3D空間上のAABB（軸平行境界ボックス）をスクリーン上の2D矩形（RECT）へ射影変換する関数
    RECT ProjectAABBToScreen(const Vector3& minBounds, const Vector3& maxBounds, const Matrix& view, const Matrix& proj,
                             float screenWidth, float screenHeight)
    {
        // 8つの頂点座標を定義
        const Vector3 corners[8] = {
            Vector3(minBounds.x, minBounds.y, minBounds.z), Vector3(maxBounds.x, minBounds.y, minBounds.z),
            Vector3(minBounds.x, maxBounds.y, minBounds.z), Vector3(maxBounds.x, maxBounds.y, minBounds.z),
            Vector3(minBounds.x, minBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
            Vector3(minBounds.x, maxBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
        };

        float minX = screenWidth;
        float minY = screenHeight;
        float maxX = 0.0f;
        float maxY = 0.0f;

        const Matrix viewProj = view * proj;

        for (int i = 0; i < 8; ++i)
        {
            Vector4 clip = Vector4::Transform(Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f), viewProj);
            if (clip.w != 0.0f)
            {
                float ndcX = clip.x / clip.w;
                float ndcY = clip.y / clip.w;

                float screenX =
                    (ndcX + EditorStyleConstants::NDC_CENTER) * EditorStyleConstants::SCREEN_CENTER_RATIO * screenWidth;
                float screenY = (EditorStyleConstants::NDC_CENTER - ndcY) * EditorStyleConstants::SCREEN_CENTER_RATIO *
                                screenHeight;

                minX = (std::min)(minX, screenX);
                maxX = (std::max)(maxX, screenX);
                minY = (std::min)(minY, screenY);
                maxY = (std::max)(maxY, screenY);
            }
        }

        return RECT{static_cast<LONG>(minX), static_cast<LONG>(minY), static_cast<LONG>(maxX), static_cast<LONG>(maxY)};
    }
}

// エディタシーン開始時のリソース初期化
void EditorScene::OnEnter(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_field.Initialize(device, context, m_gameLogic.GetBoardWidth(), m_gameLogic.GetBoardHeight(),
                       EditorConstants::DEFAULT_GRID_CELL_SIZE);
    m_gameLogic.Initialize(device, context);

    m_field.SetFloorColor(gameContext.floorColor);
    m_transitionToPlayScene = false;
    m_isPlaying = false;

    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_fxFactory->SetDirectory(L"Resources/Models");

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionNormalTexture>>(context);

    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetVertexColorEnabled(false);
    m_effect->SetLightingEnabled(false);

    void const* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    device->CreateInputLayout(VertexPositionNormalTexture::InputElements,
                              VertexPositionNormalTexture::InputElementCount, shaderByteCode, byteCodeLength,
                              m_inputLayout.ReleaseAndGetAddressOf());

    LoadSceneFromJson(gameContext, m_currentFilePath);
}

// 2Dスプライト用四角形（板ポリゴン）の描画
void EditorScene::DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context)
{
    (void)context;

    const VertexPositionNormalTexture v1(XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 0.0f));
    const VertexPositionNormalTexture v2(XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 0.0f));
    const VertexPositionNormalTexture v3(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 1.0f));
    const VertexPositionNormalTexture v4(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 1.0f));

    m_primitiveBatch->Begin();
    m_primitiveBatch->DrawTriangle(v1, v2, v3);
    m_primitiveBatch->DrawTriangle(v2, v4, v3);
    m_primitiveBatch->End();
}

// テクスチャロードおよびキャッシュ取得
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> EditorScene::GetOrLoadTexture(ID3D11Device* device,
                                                                               const std::string& texName)
{
    auto it = m_textureCache.find(texName);
    if (it != m_textureCache.end())
        return it->second;

    std::string fullPathStr = "Resources/Textures/" + texName;
    std::wstring fullPath(fullPathStr.begin(), fullPathStr.end());

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = DirectX::CreateWICTextureFromFile(device, fullPath.c_str(), nullptr, srv.GetAddressOf());

    if (FAILED(hr))
    {
        fullPathStr = "Resources/Models/" + texName;
        std::wstring fullPath2(fullPathStr.begin(), fullPathStr.end());
        hr = DirectX::CreateWICTextureFromFile(device, fullPath2.c_str(), nullptr, srv.GetAddressOf());
    }

    if (SUCCEEDED(hr))
    {
        m_textureCache[texName] = srv;
        return srv;
    }

    return nullptr;
}

// 選択中オブジェクトのコピー
void EditorScene::CopySelectedObject()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_placedObjects.size()))
    {
        m_clipboardObject = m_placedObjects[m_selectedIndex];
        m_hasClipboard = true;
    }
}

// クリップボードデータの貼り付け
void EditorScene::PasteObject()
{
    if (!m_hasClipboard)
        return;

    PlacedObject newObj = m_clipboardObject;
    newObj.name = newObj.cmoFileName + "_" + std::to_string(m_placedObjects.size() + 1);

    newObj.position.x += EditorConstants::DUP_OFFSET_X;
    newObj.position.y += EditorConstants::DUP_OFFSET_Y;

    m_placedObjects.push_back(newObj);
    m_selectedIndex = static_cast<int>(m_placedObjects.size()) - 1;
}

// オブジェクトの複製
void EditorScene::DuplicateObject(int index)
{
    if (index >= 0 && index < static_cast<int>(m_placedObjects.size()))
    {
        PlacedObject newObj = m_placedObjects[index];
        newObj.name = newObj.cmoFileName + "_" + std::to_string(m_placedObjects.size() + 1);
        newObj.position.x += EditorConstants::DUP_OFFSET_X;
        newObj.position.y += EditorConstants::DUP_OFFSET_Y;

        m_placedObjects.push_back(newObj);
        m_selectedIndex = static_cast<int>(m_placedObjects.size()) - 1;
    }
}

// 毎フレームの更新処理
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
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float viewWidth = (std::max)(1.0f, viewport->WorkSize.x - EditorStyleConstants::RIGHT_PANEL_WIDTH);
    float viewHeight = viewport->WorkSize.y;

    if (!io.WantCaptureKeyboard)
    {
        bool ctrlPressed = io.KeyCtrl;

        if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_C))
            CopySelectedObject();
        if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_V))
            PasteObject();
        if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            if (m_selectedIndex >= 0)
                DuplicateObject(m_selectedIndex);
        }
    }

    if (m_isPlaying && m_editorMode == EditorConstants::MODE_SCENE_OBJECTS)
    {
        for (const auto& obj : m_placedObjects)
        {
            std::string nameLower = obj.cmoFileName;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (obj.tag == "StartButton" || nameLower.find("start") != std::string::npos)
            {
                Vector3 minB = obj.position - (obj.scale * EditorConstants::AABB_HALF_SCALE);
                Vector3 maxB = obj.position + (obj.scale * EditorConstants::AABB_HALF_SCALE);
                m_startBtnRect = ProjectAABBToScreen(minB, maxB, m_view, m_projection, viewWidth, viewHeight);
            }
            else if (obj.tag == "ExitButton" || nameLower.find("exit") != std::string::npos)
            {
                Vector3 minB = obj.position - (obj.scale * EditorConstants::AABB_HALF_SCALE);
                Vector3 maxB = obj.position + (obj.scale * EditorConstants::AABB_HALF_SCALE);
                m_exitBtnRect = ProjectAABBToScreen(minB, maxB, m_view, m_projection, viewWidth, viewHeight);
            }
        }

        auto mouseState = DirectX::Mouse::Get().GetState();
        static DirectX::Mouse::ButtonStateTracker mouseTracker;
        mouseTracker.Update(mouseState);

        m_isStartHovered = (mouseState.x >= m_startBtnRect.left && mouseState.x <= m_startBtnRect.right &&
                            mouseState.y >= m_startBtnRect.top && mouseState.y <= m_startBtnRect.bottom);

        m_isExitHovered = (mouseState.x >= m_exitBtnRect.left && mouseState.x <= m_exitBtnRect.right &&
                           mouseState.y >= m_exitBtnRect.top && mouseState.y <= m_exitBtnRect.bottom);

        if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
        {
            if (m_isStartHovered)
                m_transitionToPlayScene = true;
            else if (m_isExitHovered)
                PostQuitMessage(0);
        }
        return;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        m_cameraYaw += io.MouseDelta.x * m_cameraRotateSpeed;
        m_cameraPitch += io.MouseDelta.y * m_cameraRotateSpeed;

        constexpr float pitchLimit = XM_PIDIV2 - EditorConstants::CAMERA_PITCH_LIMIT_MARGIN;
        m_cameraPitch = std::clamp(m_cameraPitch, -pitchLimit, pitchLimit);
    }

    Matrix camRot = Matrix::CreateRotationX(m_cameraPitch) * Matrix::CreateRotationY(m_cameraYaw);
    Vector3 forward = Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), camRot);
    Vector3 right = Vector3::TransformNormal(Vector3(1.0f, 0.0f, 0.0f), camRot);
    Vector3 up = Vector3::Up;

    if (!io.WantCaptureKeyboard)
    {
        float speed = m_cameraMoveSpeed * dt;
        if (io.KeyShift)
            speed *= EditorConstants::FAST_CAMERA_MOVE_SPEED_SCALE;

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
        m_cameraPos += forward * (io.MouseWheel * EditorConstants::MOUSE_WHEEL_ZOOM_SCALE);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F))
    {
        if (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS)
        {
            m_cameraPos = Vector3(0.0f, 0.0f, EditorStyleConstants::CAM_RESET_2D_POS_Z);
            m_cameraYaw = 0.0f;
            m_cameraPitch = 0.0f;
        }
        else
        {
            m_cameraPos =
                Vector3(0.0f, EditorStyleConstants::CAM_RESET_3D_POS_Y, EditorStyleConstants::CAM_RESET_3D_POS_Z);
            Vector3 dir = Vector3::Zero - m_cameraPos;
            dir.Normalize();
            m_cameraYaw = std::atan2(dir.x, dir.z);
            m_cameraPitch = -std::asin(dir.y);
        }
    }

    if (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !io.WantCaptureMouse)
    {
        Vector2 mousePos(io.MousePos.x, io.MousePos.y);

        if (mousePos.x <= viewWidth)
        {
            float x = (2.0f * mousePos.x) / viewWidth - 1.0f;
            float y = 1.0f - (2.0f * mousePos.y) / viewHeight;
            Matrix invViewProj = (m_view * m_projection).Invert();

            Vector4 nearPoint = Vector4::Transform(Vector4(x, y, 0.0f, 1.0f), invViewProj);
            Vector4 farPoint = Vector4::Transform(Vector4(x, y, 1.0f, 1.0f), invViewProj);

            Vector3 rayOrigin =
                Vector3(nearPoint.x / nearPoint.w, nearPoint.y / nearPoint.w, nearPoint.z / nearPoint.w);
            Vector3 rayFar = Vector3(farPoint.x / farPoint.w, farPoint.y / farPoint.w, farPoint.z / farPoint.w);
            Vector3 rayDir = rayFar - rayOrigin;
            rayDir.Normalize();

            float closestT = FLT_MAX;
            int hitIndex = -1;

            for (size_t i = 0; i < m_placedObjects.size(); ++i)
            {
                const auto& obj = m_placedObjects[i];
                Vector3 minBounds = obj.position - (obj.scale * EditorConstants::AABB_HALF_SCALE);
                Vector3 maxBounds = obj.position + (obj.scale * EditorConstants::AABB_HALF_SCALE);

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
    }
}

// 毎フレームの描画処理
void EditorScene::Render(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float fullWidth = static_cast<float>(outputSize.right - outputSize.left);
    float fullHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y);

    float viewWidth = (std::max)(1.0f, workSize.x - EditorStyleConstants::RIGHT_PANEL_WIDTH);
    float viewHeight = workSize.y;
    float aspectRatio = viewWidth / viewHeight;

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = viewport->WorkPos.y;
    vp.Width = viewWidth;
    vp.Height = viewHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    Matrix camRot = Matrix::CreateRotationX(m_cameraPitch) * Matrix::CreateRotationY(m_cameraYaw);
    Vector3 forward = Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), camRot);

    Matrix rootWorld = Matrix::CreateRotationX(XMConvertToRadians(m_sceneRootRotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(m_sceneRootRotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(m_sceneRootRotation.z)) *
                       Matrix::CreateTranslation(m_sceneRootPosition);

    Matrix perspectiveView = Matrix::CreateLookAt(m_cameraPos, m_cameraPos + forward, Vector3::Up);
    Matrix perspectiveProj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(EditorConstants::FOV_ANGLE_Y), aspectRatio,
                                             EditorConstants::NEAR_PLANE, EditorConstants::FAR_PLANE);

    if (m_editorMode == EditorConstants::MODE_GRID)
    {
        m_field.Render(context, gameContext.commonStates, perspectiveView, perspectiveProj, -1, -1, -1, -1);
        m_gameLogic.Render(context, gameContext.commonStates, perspectiveView, perspectiveProj);
    }

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    for (size_t i = 0; i < m_placedObjects.size(); ++i)
    {
        const auto& obj = m_placedObjects[i];
        if (!obj.model)
            continue;

        std::string tagLower = obj.tag;
        std::string nameLower = obj.cmoFileName;
        std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        bool isSky = (tagLower.find("sky") != std::string::npos || nameLower.find("sky") != std::string::npos);

        Matrix localWorld;
        if (isSky)
        {
            localWorld = Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                         Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                         Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                         Matrix::CreateTranslation(m_cameraPos);

            context->RSSetState(gameContext.commonStates.CullNone());
        }
        else
        {
            localWorld = Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                         Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                         Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                         Matrix::CreateTranslation(obj.position);

            context->RSSetState(gameContext.commonStates.CullCounterClockwise());
        }

        Matrix finalWorld = localWorld * rootWorld;

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

        obj.model->Draw(context, gameContext.commonStates, finalWorld, perspectiveView, perspectiveProj);
    }

    context->ClearDepthStencilView(gameContext.deviceResources.GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    Matrix orthoView = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, -10.0f), Vector3::Zero, Vector3::Up);
    float orthoHeight = EditorConstants::ORTHO_HEIGHT;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix orthoProj =
        Matrix::CreateOrthographic(orthoWidth, orthoHeight, EditorConstants::NEAR_PLANE, EditorConstants::FAR_PLANE);

    m_view = (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS) ? orthoView : perspectiveView;
    m_projection = (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS) ? orthoProj : perspectiveProj;

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->RSSetState(gameContext.commonStates.CullNone());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);

    for (size_t i = 0; i < m_placedObjects.size(); ++i)
    {
        const auto& obj = m_placedObjects[i];
        if (obj.model)
            continue;

        Matrix localWorld =
            Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
            Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
            Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) * Matrix::CreateTranslation(obj.position);

        Matrix finalWorld = localWorld * rootWorld;

        auto tex = GetOrLoadTexture(device, obj.cmoFileName);

        if (tex)
        {
            m_effect->SetTexture(tex.Get());
            m_effect->SetWorld(finalWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);

            if (m_isPlaying &&
                ((obj.tag == "StartButton" && m_isStartHovered) || (obj.tag == "ExitButton" && m_isExitHovered)))
            {
                m_effect->SetDiffuseColor(Vector4(1.4f, 1.4f, 1.4f, 1.0f));
            }
            else if (!m_isPlaying && static_cast<int>(i) == m_selectedIndex)
            {
                m_effect->SetDiffuseColor(Vector4(1.2f, 0.9f, 0.4f, 1.0f));
            }
            else
            {
                m_effect->SetDiffuseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());

            DrawQuadWithPrimitiveBatch(context);
        }
    }

    D3D11_VIEWPORT fullVp = {};
    fullVp.TopLeftX = 0.0f;
    fullVp.TopLeftY = 0.0f;
    fullVp.Width = fullWidth;
    fullVp.Height = fullHeight;
    fullVp.MinDepth = 0.0f;
    fullVp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &fullVp);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->RSSetState(gameContext.commonStates.CullCounterClockwise());

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
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImVec2 workPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y);
    float leftAreaWidth = workSize.x - EditorStyleConstants::RIGHT_PANEL_WIDTH;

    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(ImVec2(leftAreaWidth, EditorStyleConstants::TOP_TOOLBAR_HEIGHT));
    ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##TopToolbar", nullptr, toolbarFlags);
    {
        bool isGridMode = (m_editorMode == EditorConstants::MODE_GRID);
        bool isObjMode = (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS);

        if (isGridMode)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Grid Mode"))
            m_editorMode = EditorConstants::MODE_GRID;
        if (isGridMode)
            ImGui::PopStyleColor();

        ImGui::SameLine();

        if (isObjMode)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Scene Objects Mode"))
            m_editorMode = EditorConstants::MODE_SCENE_OBJECTS;
        if (isObjMode)
            ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        if (m_isPlaying)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button(" Stop Preview "))
                m_isPlaying = false;
            ImGui::PopStyleColor();
        }
        else
        {
            if (ImGui::Button(" Play Preview "))
                m_isPlaying = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("| Target: %s", m_currentFilePath.c_str());

        ImGui::SameLine();
        if (ImGui::Button("Save"))
            SaveSceneToJson(m_currentFilePath);
    }
    ImGui::End();

    float rightPanelHeightTop = workSize.y * EditorStyleConstants::RIGHT_PANEL_TOP_RATIO;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + leftAreaWidth, workPos.y));
    ImGui::SetNextWindowSize(ImVec2(EditorStyleConstants::RIGHT_PANEL_WIDTH, rightPanelHeightTop));

    ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        if (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS)
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
                    ImGui::Text("Tag");
                    ImGui::TableSetColumnIndex(1);
                    char tagBuf[EditorConstants::TAG_BUFFER_SIZE];
                    strcpy_s(tagBuf, selObj.tag.c_str());
                    if (ImGui::InputText("##Tag", tagBuf, sizeof(tagBuf)))
                        selObj.tag = tagBuf;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Position");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Pos", &selObj.position.x, EditorStyleConstants::DRAG_POS_SPEED);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Rotation");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Rot", &selObj.rotation.x, EditorStyleConstants::DRAG_ROT_SPEED);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Scale");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::DragFloat3("##Scale", &selObj.scale.x, EditorStyleConstants::DRAG_SCALE_SPEED);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Deselect", ImVec2(-1.0f, 0.0f)))
                    m_selectedIndex = -1;
            }
            else
            {
                ImGui::TextDisabled("No object selected.");
            }
        }
        else
        {
            ImGui::Text("--- Field & Dice Settings ---");
            static int selectedX = 0, selectedY = 0;
            int maxW = m_gameLogic.GetBoardWidth() - 1;
            int maxH = m_gameLogic.GetBoardHeight() - 1;

            ImGui::SliderInt("Grid X", &selectedX, 0, maxW);
            ImGui::SliderInt("Grid Y", &selectedY, 0, maxH);
        }
    }
    ImGui::End();

    float rightPanelHeightBottom = workSize.y - rightPanelHeightTop;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + leftAreaWidth, workPos.y + rightPanelHeightTop));
    ImGui::SetNextWindowSize(ImVec2(EditorStyleConstants::RIGHT_PANEL_WIDTH, rightPanelHeightBottom));

    ImGui::Begin("Project View", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        if (ImGui::BeginTabBar("ProjectTabs"))
        {
            if (m_editorMode == EditorConstants::MODE_SCENE_OBJECTS)
            {
                if (ImGui::BeginTabItem("Hierarchy"))
                {
                    ImGui::TextDisabled("Drop .cmo / .png / .jpg into window to add");
                    ImGui::Separator();

                    if (ImGui::Button("Clear All Objects", ImVec2(-1.0f, 0.0f)))
                    {
                        m_placedObjects.clear();
                        m_selectedIndex = -1;
                    }

                    ImGui::Separator();
                    ImGui::BeginChild("ListRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

                    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight |
                                                                                   ImGuiPopupFlags_NoOpenOverItems))
                    {
                        if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, m_hasClipboard))
                            PasteObject();
                        ImGui::EndPopup();
                    }

                    int deleteIndex = -1;
                    for (size_t i = 0; i < m_placedObjects.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        bool isSelected = (static_cast<int>(i) == m_selectedIndex);

                        std::string label = m_placedObjects[i].name + " [" + m_placedObjects[i].tag + "]";
                        if (ImGui::Selectable(label.c_str(), isSelected))
                        {
                            m_selectedIndex = static_cast<int>(i);
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                                DuplicateObject(static_cast<int>(i));
                            if (ImGui::MenuItem("Copy", "Ctrl+C"))
                                CopySelectedObject();
                            if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, m_hasClipboard))
                                PasteObject();
                            ImGui::Separator();
                            if (ImGui::MenuItem("Delete", "Delete"))
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

            if (ImGui::BeginTabItem("Scenes"))
            {
                ImGui::TextDisabled("Double-click .json to load & edit");
                ImGui::Separator();

                ImGui::BeginChild("SceneListRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

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
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// JSONファイルからのシーンデータ読み込み
void EditorScene::LoadSceneFromJson(GameContext& gameContext, const std::string& filePath)
{
    SceneData loadedData;
    if (!SceneSerializer::LoadFromJson(filePath, loadedData))
        return;

    m_currentFilePath = filePath;

    if (filePath.find("title_scene.json") != std::string::npos ||
        filePath.find("select_scene.json") != std::string::npos)
    {
        m_editorMode = EditorConstants::MODE_SCENE_OBJECTS;
        m_cameraPos = Vector3(0.0f, 0.0f, EditorStyleConstants::CAM_RESET_2D_POS_Z);
        m_cameraYaw = 0.0f;
        m_cameraPitch = 0.0f;
    }
    else
    {
        m_editorMode = EditorConstants::MODE_GRID;
        m_cameraPos = Vector3(0.0f, EditorStyleConstants::CAM_RESET_3D_POS_Y, EditorStyleConstants::CAM_RESET_3D_POS_Z);
        Vector3 dir = Vector3::Zero - m_cameraPos;
        dir.Normalize();
        m_cameraYaw = std::atan2(dir.x, dir.z);
        m_cameraPitch = -std::asin(dir.y);
    }

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
        }
        catch (...)
        {
        }

        m_placedObjects.push_back(newObj);
    }
}

// JSONファイルへのシーンデータ保存
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

// 外部ファイルのドラッグ＆ドロップ受け取りハンドラー
void EditorScene::OnFileDropped(GameContext& gameContext, const std::wstring& fullPath)
{
    std::filesystem::path path(fullPath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto device = gameContext.deviceResources.GetD3DDevice();

    if (ext == ".json")
    {
        LoadSceneFromJson(gameContext, path.string());
        return;
    }

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
    {
        PlacedObject newObj;
        newObj.cmoFileName = path.filename().string();
        newObj.name = path.stem().string();

        std::string lowerStem = path.stem().string();
        std::transform(lowerStem.begin(), lowerStem.end(), lowerStem.begin(), ::tolower);

        if (lowerStem.find("start") != std::string::npos)
            newObj.tag = "StartButton";
        else if (lowerStem.find("exit") != std::string::npos)
            newObj.tag = "ExitButton";
        else
            newObj.tag = "TitleLogo";

        newObj.position = Vector3(0.0f, 0.0f, 0.0f);
        newObj.rotation = Vector3(0.0f, 0.0f, 0.0f);
        newObj.scale = Vector3(EditorStyleConstants::DEFAULT_2D_SCALE_X, EditorStyleConstants::DEFAULT_2D_SCALE_Y,
                               EditorStyleConstants::DEFAULT_2D_SCALE_Z);

        m_placedObjects.push_back(newObj);
        m_selectedIndex = static_cast<int>(m_placedObjects.size()) - 1;
        return;
    }

    if (ext == ".cmo")
    {
        try
        {
            PlacedObject newObj;
            newObj.cmoFileName = path.filename().string();
            newObj.name = newObj.cmoFileName + "_" + std::to_string(m_placedObjects.size() + 1);

            std::string lowerFileName = newObj.cmoFileName;
            std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
            if (lowerFileName.find("sky") != std::string::npos)
            {
                newObj.tag = "Sky";
                newObj.scale = Vector3(EditorStyleConstants::SKY_MODEL_SCALE, EditorStyleConstants::SKY_MODEL_SCALE,
                                       EditorStyleConstants::SKY_MODEL_SCALE);
            }
            else
            {
                newObj.tag = "Default";
                newObj.scale = Vector3(1.0f, 1.0f, 1.0f);
            }

            newObj.model = Model::CreateFromCMO(device, fullPath.c_str(), *m_fxFactory);
            newObj.position = Vector3(0.0f, 0.0f, 0.0f);
            newObj.rotation = Vector3(0.0f, 0.0f, 0.0f);

            m_placedObjects.push_back(newObj);
            m_selectedIndex = static_cast<int>(m_placedObjects.size()) - 1;
        }
        catch (...)
        {
        }
    }
}

// レイとAABBの交差判定処理 (Slab Method)
bool EditorScene::RayIntersectsAABB(const Vector3& rayOrigin, const Vector3& rayDir, const Vector3& boxMin,
                                    const Vector3& boxMax, float& outT)
{
    float tmin = (boxMin.x - rayOrigin.x) /
                 (std::abs(rayDir.x) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.x);
    float tmax = (boxMax.x - rayOrigin.x) /
                 (std::abs(rayDir.x) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.x);

    if (tmin > tmax)
        std::swap(tmin, tmax);

    float tymin = (boxMin.y - rayOrigin.y) /
                  (std::abs(rayDir.y) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.y);
    float tymax = (boxMax.y - rayOrigin.y) /
                  (std::abs(rayDir.y) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.y);

    if (tymin > tymax)
        std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;
    if (tymax < tmax)
        tmax = tymax;

    float tzmin = (boxMin.z - rayOrigin.z) /
                  (std::abs(rayDir.z) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.z);
    float tzmax = (boxMax.z - rayOrigin.z) /
                  (std::abs(rayDir.z) < EditorConstants::RAY_EPSILON ? EditorConstants::RAY_EPSILON : rayDir.z);

    if (tzmin > tzmax)
        std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    outT = tmin;
    return outT >= 0.0f;
}