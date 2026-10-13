#include "pch.h"
#include "TitleScene.h"
#include "Framework/SceneSerializer.h"
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
    RECT ProjectAABBToScreen(const Vector3& minBounds, const Vector3& maxBounds, const Matrix& view, const Matrix& proj,
                             float screenWidth, float screenHeight)
    {
        Vector3 corners[8] = {
            Vector3(minBounds.x, minBounds.y, minBounds.z), Vector3(maxBounds.x, minBounds.y, minBounds.z),
            Vector3(minBounds.x, maxBounds.y, minBounds.z), Vector3(maxBounds.x, maxBounds.y, minBounds.z),
            Vector3(minBounds.x, minBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
            Vector3(minBounds.x, maxBounds.y, maxBounds.z), Vector3(maxBounds.x, maxBounds.y, maxBounds.z),
        };

        float minX = screenWidth, minY = screenHeight;
        float maxX = 0.0f, maxY = 0.0f;

        Matrix viewProj = view * proj;

        for (int i = 0; i < 8; ++i)
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

void TitleScene::OnEnter(GameContext& gameContext)
{
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

void TitleScene::DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context, const Matrix& world)
{
    VertexPositionNormalTexture v1(XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 0.0f));
    VertexPositionNormalTexture v2(XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 0.0f));
    VertexPositionNormalTexture v3(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 1.0f));
    VertexPositionNormalTexture v4(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 1.0f));

    m_primitiveBatch->Begin();
    m_primitiveBatch->DrawTriangle(v1, v2, v3);
    m_primitiveBatch->DrawTriangle(v2, v4, v3);
    m_primitiveBatch->End();
}

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

void TitleScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    // ★ Tab + E キーでエディターシーンへ移動
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

    // EditorScene と寸法を完全一致させた Orthographic カメラ
    Matrix view = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, -10.0f), Vector3::Zero, Vector3::Up);
    float orthoHeight = 10.0f;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix projection = Matrix::CreateOrthographic(orthoWidth, orthoHeight, 0.1f, 1000.0f);

    bool foundStart = false;
    bool foundExit = false;

    for (const auto& obj : m_placedObjects)
    {
        std::string nameLower = obj.cmoFileName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (obj.tag == "StartButton" || nameLower.find("start") != std::string::npos)
        {
            Vector3 minB = obj.position - (obj.scale * 0.5f);
            Vector3 maxB = obj.position + (obj.scale * 0.5f);
            m_startBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
            foundStart = true;
        }
        else if (obj.tag == "ExitButton" || nameLower.find("exit") != std::string::npos)
        {
            Vector3 minB = obj.position - (obj.scale * 0.5f);
            Vector3 maxB = obj.position + (obj.scale * 0.5f);
            m_exitBtnRect = ProjectAABBToScreen(minB, maxB, view, projection, screenWidth, screenHeight);
            foundExit = true;
        }
    }

    if (!foundStart)
    {
        float centerX = screenWidth * 0.5f, startY = screenHeight * 0.60f;
        m_startBtnRect = {static_cast<LONG>(centerX - 100.0f), static_cast<LONG>(startY - 25.0f),
                          static_cast<LONG>(centerX + 100.0f), static_cast<LONG>(startY + 25.0f)};
    }
    if (!foundExit)
    {
        float centerX = screenWidth * 0.5f, exitY = screenHeight * 0.72f;
        m_exitBtnRect = {static_cast<LONG>(centerX - 100.0f), static_cast<LONG>(exitY - 25.0f),
                         static_cast<LONG>(centerX + 100.0f), static_cast<LONG>(exitY + 25.0f)};
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
        {
            sceneController.RequestSwitch(SceneId::SelectScene);
        }
        else if (m_isExitHovered)
        {
            PostQuitMessage(0);
        }
    }
}

void TitleScene::Render(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    // ==========================================
    // レイヤー 1: 3Dワールド空間（Perspective）
    // スカイドーム等の 3Dモデルを描画
    // ==========================================
    Vector3 cameraPos = Vector3(0.0f, 0.0f, -10.0f);
    Matrix perspectiveView = Matrix::CreateLookAt(cameraPos, Vector3::Zero, Vector3::Up);
    Matrix perspectiveProj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 1000.0f);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    for (const auto& obj : m_placedObjects)
    {
        if (!obj.model)
            continue; // 3Dモデル以外はスキップ

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

    // ==========================================
    // レイヤー 2: 2D Canvas UI（Orthographic）
    // 3D背景描画後に深度バッファをクリアし、UIを最前面に描画
    // ==========================================
    context->ClearDepthStencilView(gameContext.deviceResources.GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    Matrix orthoView = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, -10.0f), Vector3::Zero, Vector3::Up);
    float orthoHeight = 10.0f;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix orthoProj = Matrix::CreateOrthographic(orthoWidth, orthoHeight, 0.1f, 1000.0f);

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->RSSetState(gameContext.commonStates.CullNone());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);

    for (const auto& obj : m_placedObjects)
    {
        if (obj.model)
            continue; // 3Dモデルはレイヤー1で描画済み

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

            if ((obj.tag == "StartButton" && m_isStartHovered) || (obj.tag == "ExitButton" && m_isExitHovered))
            {
                m_effect->SetDiffuseColor(Vector4(1.3f, 1.3f, 1.3f, 1.0f));
            }
            else
            {
                m_effect->SetDiffuseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());

            DrawQuadWithPrimitiveBatch(context, localWorld);
        }
    }

    // ステート復元
    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->RSSetState(gameContext.commonStates.CullCounterClockwise());

    // デバッグテキスト描画
    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    debugRenderer.Render(context, &gameContext.commonStates);
}