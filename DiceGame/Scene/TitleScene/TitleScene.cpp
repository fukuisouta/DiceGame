#include "pch.h"
#include "TitleScene.h"
#include "Framework/SceneSerializer.h"
#include "Network/NetworkPackets.h"

#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// タイトルシーンスタイル定数定義
namespace TitleStyleConstants
{
    constexpr float CAMERA_Z_POS = -10.0f;                                     // カメラのZ位置座標
    constexpr int CLICK_PARTICLE_COUNT = 20;                                   // クリック発生粒子数
    constexpr int AABB_CORNER_COUNT = 8;                                       // AABBの頂点数
    constexpr Vector4 HOVER_COLOR_HIGHLIGHT = Vector4(1.3f, 1.3f, 1.3f, 1.0f); // ホバー強調時カラー
    constexpr Vector4 NORMAL_COLOR = Vector4(1.0f, 1.0f, 1.0f, 1.0f);          // 通常時カラー
}

namespace
{
    // 3Dオブジェクトの有向境界ボックス（AABB）を投影しスクリーンの2D領域（RECT）に変換する内部ヘルパー関数
    RECT ProjectAABBToScreen(const Vector3& minBounds, const Vector3& maxBounds, const Matrix& view, const Matrix& proj,
                             float screenWidth, float screenHeight)
    {
        Vector3 corners[TitleStyleConstants::AABB_CORNER_COUNT] = {
            Vector3(minBounds.x, minBounds.y, minBounds.z), Vector3(maxBounds.x, minBounds.y, minBounds.z),
            Vector3(minBounds.x, maxBounds.y, minBounds.z), Vector3(maxBounds.x, maxBounds.y, minBounds.z),
            Vector3(minBounds.x, minBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
            Vector3(minBounds.x, maxBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
        };

        float minX = screenWidth, minY = screenHeight;
        float maxX = 0.0f, maxY = 0.0f;

        Matrix viewProj = view * proj;

        for (int i = 0; i < TitleStyleConstants::AABB_CORNER_COUNT; ++i)
        {
            Vector4 clip = Vector4::Transform(Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f), viewProj);
            if (clip.w != 0.0f)
            {
                float ndcX = clip.x / clip.w;
                float ndcY = clip.y / clip.w;

                float screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
                float screenY = (1.0f - ndcY) * 0.5f * screenHeight;

                minX = (std::min)(minX, screenX);
                maxX = (std::max)(maxX, screenX);
                minY = (std::min)(minY, screenY);
                maxY = (std::max)(maxY, screenY);
            }
        }

        return RECT{static_cast<LONG>(minX), static_cast<LONG>(minY), static_cast<LONG>(maxX), static_cast<LONG>(maxY)};
    }
}

// シーン切り替え時の初期化処理
void TitleScene::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Title, true);

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_fxFactory->SetDirectory(L"Resources/Models");

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionNormalTexture>>(context);

    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetVertexColorEnabled(false);
    m_effect->SetLightingEnabled(false);

    void const* shaderByteCode;
    size_t byteCodeLength;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    device->CreateInputLayout(VertexPositionNormalTexture::InputElements,
                              VertexPositionNormalTexture::InputElementCount, shaderByteCode, byteCodeLength,
                              m_inputLayout.ReleaseAndGetAddressOf());

    m_particleManager.Initialize(device, context);
    m_particleManager.Clear();

    SceneData loadedData;
    m_placedObjects.clear();

    if (SceneSerializer::LoadFromJson("Resources/Data/title_scene.json", loadedData))
    {
        for (const auto& objData : loadedData.objects)
        {
            PlacedObject newObj;
            newObj.cmoFileName = objData.modelName;
            newObj.tag = objData.tag;
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
}

// 2D画像表示用の単位クワッドポリゴン描画処理
void TitleScene::DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context, const Matrix& world)
{
    (void)context;
    (void)world;
    VertexPositionNormalTexture v1(XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 0.0f));
    VertexPositionNormalTexture v2(XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 0.0f));
    VertexPositionNormalTexture v3(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 1.0f));
    VertexPositionNormalTexture v4(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 1.0f));

    m_primitiveBatch->Begin();
    m_primitiveBatch->DrawTriangle(v1, v2, v3);
    m_primitiveBatch->DrawTriangle(v2, v4, v3);
    m_primitiveBatch->End();
}

// テクスチャリソースの検索・ロード・キャッシュ処理
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TitleScene::GetOrLoadTexture(ID3D11Device* device,
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

// 毎フレームのロジック・判定更新
void TitleScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    SoundManager::GetInstance().Update();

    m_particleManager.Update(dt);

    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Tab) && kbState.IsKeyDown(DirectX::Keyboard::Keys::E))
    {
        sceneController.RequestSwitch(SceneId::EditorScene);
        return;
    }

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Matrix view =
        Matrix::CreateLookAt(Vector3(0.0f, 0.0f, TitleStyleConstants::CAMERA_Z_POS), Vector3::Zero, Vector3::Up);
    float orthoHeight = TitleConstants::ORTHO_HEIGHT;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix projection = Matrix::CreateOrthographic(orthoWidth, orthoHeight, TitleConstants::CAMERA_NEAR_Z,
                                                   TitleConstants::CAMERA_FAR_Z);

    for (const auto& obj : m_placedObjects)
    {
        std::string nameLower = obj.cmoFileName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (obj.tag == "SoloButton" || obj.tag == "StartButton" || nameLower.find("solo") != std::string::npos ||
            nameLower.find("start") != std::string::npos)
        {
            Vector3 minB = obj.position - (obj.scale * 0.5f);
            Vector3 maxB = obj.position + (obj.scale * 0.5f);
            m_soloBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
        }
        else if (obj.tag == "MultiButton" || nameLower.find("multi") != std::string::npos)
        {
            Vector3 minB = obj.position - (obj.scale * 0.5f);
            Vector3 maxB = obj.position + (obj.scale * 0.5f);
            m_multiBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
        }
        else if (obj.tag == "ExitButton" || nameLower.find("exit") != std::string::npos)
        {
            Vector3 minB = obj.position - (obj.scale * 0.5f);
            Vector3 maxB = obj.position + (obj.scale * 0.5f);
            m_exitBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
        }
    }

    auto mouseState = DirectX::Mouse::Get().GetState();
    static DirectX::Mouse::ButtonStateTracker mouseTracker;
    mouseTracker.Update(mouseState);

    m_isSoloHovered = (mouseState.x >= m_soloBtnRect.left && mouseState.x <= m_soloBtnRect.right &&
                       mouseState.y >= m_soloBtnRect.top && mouseState.y <= m_soloBtnRect.bottom);

    m_isMultiHovered = (mouseState.x >= m_multiBtnRect.left && mouseState.x <= m_multiBtnRect.right &&
                        mouseState.y >= m_multiBtnRect.top && mouseState.y <= m_multiBtnRect.bottom);

    m_isExitHovered = (mouseState.x >= m_exitBtnRect.left && mouseState.x <= m_exitBtnRect.right &&
                       mouseState.y >= m_exitBtnRect.top && mouseState.y <= m_exitBtnRect.bottom);

    if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
    {
        m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                              TitleStyleConstants::CLICK_PARTICLE_COUNT);

        if (m_isSoloHovered)
        {
            gameContext.networkManager.Shutdown();
            sceneController.RequestSwitch(SceneId::SelectScene);
        }
        else if (m_isMultiHovered)
        {
            sceneController.RequestSwitch(SceneId::LobbyScene);
        }
        else if (m_isExitHovered)
        {
            PostQuitMessage(0);
        }
    }
}

// レンダリング描画処理
void TitleScene::Render(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Vector3 cameraPos = Vector3(0.0f, 0.0f, TitleStyleConstants::CAMERA_Z_POS);
    Matrix perspectiveView = Matrix::CreateLookAt(cameraPos, Vector3::Zero, Vector3::Up);
    Matrix perspectiveProj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(TitleConstants::DEFAULT_FOV_DEGREE), aspectRatio,
                                             TitleConstants::CAMERA_NEAR_Z, TitleConstants::CAMERA_FAR_Z);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    for (const auto& obj : m_placedObjects)
    {
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
                         Matrix::CreateTranslation(cameraPos);

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

        obj.model->Draw(context, gameContext.commonStates, localWorld, perspectiveView, perspectiveProj);
    }

    context->ClearDepthStencilView(gameContext.deviceResources.GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    Matrix orthoView =
        Matrix::CreateLookAt(Vector3(0.0f, 0.0f, TitleStyleConstants::CAMERA_Z_POS), Vector3::Zero, Vector3::Up);
    float orthoHeight = TitleConstants::ORTHO_HEIGHT;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix orthoProj = Matrix::CreateOrthographic(orthoWidth, orthoHeight, TitleConstants::CAMERA_NEAR_Z,
                                                  TitleConstants::CAMERA_FAR_Z);

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->RSSetState(gameContext.commonStates.CullNone());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);

    for (const auto& obj : m_placedObjects)
    {
        if (obj.model)
            continue;

        std::string tagLower = obj.tag;
        std::string nameLower = obj.cmoFileName;
        std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        Matrix localWorld =
            Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
            Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
            Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) * Matrix::CreateTranslation(obj.position);

        auto tex = GetOrLoadTexture(device, obj.cmoFileName);

        if (tex)
        {
            m_effect->SetTexture(tex.Get());
            m_effect->SetWorld(localWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);

            bool isSolo = (tagLower.find("solo") != std::string::npos || nameLower.find("solo") != std::string::npos ||
                           tagLower.find("start") != std::string::npos);
            bool isMulti =
                (tagLower.find("multi") != std::string::npos || nameLower.find("multi") != std::string::npos);
            bool isExit = (tagLower.find("exit") != std::string::npos || nameLower.find("exit") != std::string::npos);

            if ((isSolo && m_isSoloHovered) || (isMulti && m_isMultiHovered) || (isExit && m_isExitHovered))
            {
                m_effect->SetDiffuseColor(TitleStyleConstants::HOVER_COLOR_HIGHLIGHT);
            }
            else
            {
                m_effect->SetDiffuseColor(TitleStyleConstants::NORMAL_COLOR);
            }

            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());

            DrawQuadWithPrimitiveBatch(context, localWorld);
        }
    }

    m_particleManager.Render(context, screenWidth, screenHeight, gameContext.commonStates);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->RSSetState(gameContext.commonStates.CullCounterClockwise());

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    debugRenderer.Render(context, &gameContext.commonStates);
}