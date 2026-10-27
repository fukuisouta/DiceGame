#include "pch.h"
#include "SelectScene.h"
#include "Framework/SceneSerializer.h"
#include "Network/NetworkPackets.h"

#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
    // 正射影（Ortho）空間の座標とスケールを画面座標系の矩形（RECT）へ変換するヘルパー関数
    RECT ConvertOrthoToScreenRect(Vector3 pos, Vector3 scale, float orthoWidth, float orthoHeight, float screenWidth,
                                  float screenHeight)
    {
        float leftW = pos.x - (scale.x * 0.5f);
        float rightW = pos.x + (scale.x * 0.5f);
        float topW = pos.y + (scale.y * 0.5f);
        float bottomW = pos.y - (scale.y * 0.5f);

        float x1 = (0.5f - leftW / orthoWidth) * screenWidth;
        float x2 = (0.5f - rightW / orthoWidth) * screenWidth;
        float y1 = (0.5f - topW / orthoHeight) * screenHeight;
        float y2 = (0.5f - bottomW / orthoHeight) * screenHeight;

        LONG minX = static_cast<LONG>((std::min)(x1, x2));
        LONG maxX = static_cast<LONG>((std::max)(x1, x2));
        LONG minY = static_cast<LONG>((std::min)(y1, y2));
        LONG maxY = static_cast<LONG>((std::max)(y1, y2));

        return RECT{minX, minY, maxX, maxY};
    }
}

// シーン進入時の初期化処理
void SelectScene::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Select);

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

    m_mouseTracker.Reset();

    // ★ パーティクルマネージャーの初期化とクリア
    m_particleManager.Initialize(device, context);
    m_particleManager.Clear();

    // ルール説明用の各種GIFアニメーションファイルのロード
    m_gifRoll.LoadFromFile(device, L"Resources/Textures/Roll.gif");
    m_gifMove.LoadFromFile(device, L"Resources/Textures/Move.gif");
    m_gifClash.LoadFromFile(device, L"Resources/Textures/Kill.gif");

    // ページ切替矢印テクスチャのプリロード
    GetOrLoadTexture(device, "Left.png");
    GetOrLoadTexture(device, "Right.png");

    m_showRulePanel = false;
    m_currentGuidePage = 0;

    // JSONファイルからの配置オブジェクトデータの復元
    SceneData loadedData;
    m_placedObjects.clear();

    if (SceneSerializer::LoadFromJson("Resources/Data/select_scene.json", loadedData))
    {
        for (const auto& objData : loadedData.objects)
        {
            std::string tagLower = objData.tag;
            std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
            if (tagLower == "gif_roll" || tagLower == "gif_move" || tagLower == "gif_clash" || tagLower == "roll" ||
                tagLower == "move" || tagLower == "clash" || tagLower == "kill")
            {
                continue;
            }

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

// 単位クワッド（板ポリゴン）の描画
void SelectScene::DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context)
{
    (void)context;
    VertexPositionNormalTexture v1(XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 0.0f));
    VertexPositionNormalTexture v2(XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 0.0f));
    VertexPositionNormalTexture v3(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0.0f, 1.0f));
    VertexPositionNormalTexture v4(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1.0f, 1.0f));

    m_primitiveBatch->Begin();
    m_primitiveBatch->DrawTriangle(v1, v2, v3);
    m_primitiveBatch->DrawTriangle(v2, v4, v3);
    m_primitiveBatch->End();
}

// テクスチャのキャッシュ付きロードヘルパー関数
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SelectScene::GetOrLoadTexture(ID3D11Device* device,
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

// 毎フレームのロジック・入力・ネットワークパケット同期更新
void SelectScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());

    // ★ パーティクルの物理・寿命更新
    m_particleManager.Update(dt);

    // オンライン対戦時：ホストからのモード選択パケット受信検知および同期シーン遷移
    if (gameContext.networkManager.IsConnected())
    {
        std::vector<uint8_t> packet;
        while (gameContext.networkManager.PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;
            PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

            if (header->type == PacketType::CS_SelectMode)
            {
                MsgSelectMode* msg = reinterpret_cast<MsgSelectMode*>(packet.data());
                if (msg->mode == 0)
                {
                    gameContext.selectedMode = GameMode::Original;
                    sceneController.RequestSwitch(SceneId::OriginalGamePlayScene);
                }
                else
                {
                    gameContext.selectedMode = GameMode::Advance;
                    sceneController.RequestSwitch(SceneId::AdvanceGamePlayScene);
                }
                return;
            }
        }
    }

    // デバッグ・エディターシーンへのショートカットキー（Tab + E）
    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Tab) && kbState.IsKeyDown(DirectX::Keyboard::Keys::E))
    {
        sceneController.RequestSwitch(SceneId::EditorScene);
        return;
    }

    // ルールガイドアニメーション（GIF）のコマ更新
    if (m_showRulePanel)
    {
        if (m_currentGuidePage == 0)
            m_gifRoll.Update(dt);
        else if (m_currentGuidePage == 1)
            m_gifMove.Update(dt);
        else if (m_currentGuidePage == 2)
            m_gifClash.Update(dt);
    }

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    float orthoHeight = 10.0f;
    float orthoWidth = orthoHeight * aspectRatio;

    // オブジェクトタグに基づき各UIボタンの画面領域（RECT）を算出
    for (const auto& obj : m_placedObjects)
    {
        std::string tagLower = obj.tag;
        std::string nameLower = obj.cmoFileName;
        std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (tagLower == "originalbutton" || tagLower == "original" || nameLower.find("original") != std::string::npos)
        {
            m_originalBtnRect =
                ConvertOrthoToScreenRect(obj.position, obj.scale, orthoWidth, orthoHeight, screenWidth, screenHeight);
        }
        else if (tagLower == "advancebutton" || tagLower == "advance" || nameLower.find("advance") != std::string::npos)
        {
            m_advanceBtnRect =
                ConvertOrthoToScreenRect(obj.position, obj.scale, orthoWidth, orthoHeight, screenWidth, screenHeight);
        }
        // エディタで配置した GUIDE / RULE タグのオブジェクトから領域を算出
        else if (tagLower == "guide" || tagLower == "rule" || nameLower.find("guide") != std::string::npos ||
                 nameLower.find("rule") != std::string::npos)
        {
            m_ruleBtnRect =
                ConvertOrthoToScreenRect(obj.position, obj.scale, orthoWidth, orthoHeight, screenWidth, screenHeight);
        }
    }

    // ページ切り替え矢印ボタンの領域計算
    Vector3 leftArrowPos(-4.5f, -0.2f, 0.0f);
    Vector3 rightArrowPos(4.5f, -0.2f, 0.0f);
    Vector3 arrowScale(1.3f, 1.3f, 1.0f);

    m_prevArrowRect =
        ConvertOrthoToScreenRect(leftArrowPos, arrowScale, orthoWidth, orthoHeight, screenWidth, screenHeight);
    m_nextArrowRect =
        ConvertOrthoToScreenRect(rightArrowPos, arrowScale, orthoWidth, orthoHeight, screenWidth, screenHeight);

    // マウスカーソルホバー判定の更新
    auto mouseState = DirectX::Mouse::Get().GetState();
    m_mouseTracker.Update(mouseState);

    m_isOriginalHovered = (mouseState.x >= m_originalBtnRect.left && mouseState.x <= m_originalBtnRect.right &&
                           mouseState.y >= m_originalBtnRect.top && mouseState.y <= m_originalBtnRect.bottom);

    m_isAdvanceHovered = (mouseState.x >= m_advanceBtnRect.left && mouseState.x <= m_advanceBtnRect.right &&
                          mouseState.y >= m_advanceBtnRect.top && mouseState.y <= m_advanceBtnRect.bottom);

    m_isRuleBtnHovered = (mouseState.x >= m_ruleBtnRect.left && mouseState.x <= m_ruleBtnRect.right &&
                          mouseState.y >= m_ruleBtnRect.top && mouseState.y <= m_ruleBtnRect.bottom);

    m_isPrevHovered = (mouseState.x >= m_prevArrowRect.left && mouseState.x <= m_prevArrowRect.right &&
                       mouseState.y >= m_prevArrowRect.top && mouseState.y <= m_prevArrowRect.bottom);

    m_isNextHovered = (mouseState.x >= m_nextArrowRect.left && mouseState.x <= m_nextArrowRect.right &&
                       mouseState.y >= m_nextArrowRect.top && mouseState.y <= m_nextArrowRect.bottom);

    // クリックイベントの処理およびパーティクル発生
    if (m_mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
    {
        // ★ 画面クリック座標にパーティクルを発生
        m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y), 20);

        if (m_isRuleBtnHovered)
        {
            m_showRulePanel = !m_showRulePanel; // ルール表示切替
        }
        else if (m_showRulePanel)
        {
            // ページ切替処理
            if (m_isPrevHovered)
            {
                m_currentGuidePage = (m_currentGuidePage + 1) % 3;
            }
            else if (m_isNextHovered)
            {
                m_currentGuidePage = (m_currentGuidePage + 2) % 3;
            }
        }
        else
        {
            // オンライン選択の送信用ラムダ式
            auto SendModePacket = [&](uint8_t modeVal)
            {
                if (gameContext.networkManager.IsConnected() &&
                    gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
                {
                    MsgSelectMode msg{};
                    msg.header.size = sizeof(MsgSelectMode);
                    msg.header.type = PacketType::CS_SelectMode;
                    msg.mode = modeVal;
                    gameContext.networkManager.SendPacket(&msg, sizeof(msg));
                }
            };

            // モード選択ボタンクリック時
            if (m_isOriginalHovered)
            {
                SendModePacket(0);
                gameContext.selectedMode = GameMode::Original;
                sceneController.RequestSwitch(SceneId::OriginalGamePlayScene);
            }
            else if (m_isAdvanceHovered)
            {
                SendModePacket(1);
                gameContext.selectedMode = GameMode::Advance;
                sceneController.RequestSwitch(SceneId::AdvanceGamePlayScene);
            }
        }
    }
}

// 画面のレンダリング描画処理
void SelectScene::Render(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Vector3 cameraPos = Vector3(0.0f, 0.0f, -10.0f);
    Matrix perspectiveView = Matrix::CreateLookAt(cameraPos, Vector3::Zero, Vector3::Up);
    Matrix perspectiveProj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 1000.0f);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    // 1. 3D背景モデルおよびスカイドームの描画
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

    // 2. 2D UIボタン（正射影描画）の更新と描画
    Matrix orthoView = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, -10.0f), Vector3::Zero, Vector3::Up);
    float orthoHeight = 10.0f;
    float orthoWidth = orthoHeight * aspectRatio;
    Matrix orthoProj = Matrix::CreateOrthographic(orthoWidth, orthoHeight, 0.1f, 1000.0f);

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->RSSetState(gameContext.commonStates.CullNone());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);

    m_effect->SetTextureEnabled(true);

    for (const auto& obj : m_placedObjects)
    {
        if (obj.model)
            continue;

        std::string tagLower = obj.tag;
        std::string nameLower = obj.cmoFileName;
        std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        bool isOriginal =
            (tagLower.find("original") != std::string::npos || nameLower.find("original") != std::string::npos);
        bool isAdvance =
            (tagLower.find("advance") != std::string::npos || nameLower.find("advance") != std::string::npos);
        bool isGuide = (tagLower.find("guide") != std::string::npos || tagLower.find("rule") != std::string::npos ||
                        nameLower.find("guide") != std::string::npos || nameLower.find("rule") != std::string::npos);

        // ホバー時の演出（少し拡大・明るくする）
        float scaleMul = 1.0f;
        if ((isOriginal && m_isOriginalHovered) || (isAdvance && m_isAdvanceHovered) || (isGuide && m_isRuleBtnHovered))
        {
            scaleMul = 1.1f;
        }

        Matrix localWorld =
            Matrix::CreateScale(obj.scale * scaleMul) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
            Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
            Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) * Matrix::CreateTranslation(obj.position);

        auto texRef = GetOrLoadTexture(device, obj.cmoFileName);
        if (texRef)
        {
            m_effect->SetTexture(texRef.Get());
            m_effect->SetWorld(localWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);

            // ホバー時・暗転時のディフューズカラー調整
            if (m_showRulePanel)
            {
                m_effect->SetDiffuseColor(Vector4(0.4f, 0.4f, 0.4f, 1.0f));
            }
            else if ((isOriginal && m_isOriginalHovered) || (isAdvance && m_isAdvanceHovered) ||
                     (isGuide && m_isRuleBtnHovered))
            {
                m_effect->SetDiffuseColor(Vector4(1.4f, 1.4f, 1.4f, 1.0f));
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

    // 3. ルール解説パネル（GIFアニメーション＆ページめくり矢印）のオーバーレイ描画
    if (m_showRulePanel)
    {
        m_effect->SetTextureEnabled(true);

        ID3D11ShaderResourceView* currentGifTex = nullptr;
        if (m_currentGuidePage == 0)
            currentGifTex = m_gifRoll.GetCurrentTexture();
        else if (m_currentGuidePage == 1)
            currentGifTex = m_gifMove.GetCurrentTexture();
        else if (m_currentGuidePage == 2)
            currentGifTex = m_gifClash.GetCurrentTexture();

        if (currentGifTex)
        {
            Matrix gifWorld = Matrix::CreateScale(-7.2f, 4.2f, 1.0f) * Matrix::CreateTranslation(0.0f, -0.2f, 0.0f);
            m_effect->SetTexture(currentGifTex);
            m_effect->SetWorld(gifWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);
            m_effect->SetDiffuseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());
            DrawQuadWithPrimitiveBatch(context);
        }

        auto leftTex = GetOrLoadTexture(device, "Left.png");
        auto rightTex = GetOrLoadTexture(device, "Right.png");

        if (leftTex)
        {
            float scaleMul = m_isPrevHovered ? 1.25f : 1.0f;
            Matrix leftWorld = Matrix::CreateScale(1.3f * scaleMul, 1.3f * scaleMul, 1.0f) *
                               Matrix::CreateTranslation(-4.5f, -0.2f, 0.0f);
            m_effect->SetTexture(leftTex.Get());
            m_effect->SetWorld(leftWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);
            m_effect->SetDiffuseColor(m_isPrevHovered ? Vector4(1.4f, 1.4f, 1.4f, 1.0f)
                                                      : Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());
            DrawQuadWithPrimitiveBatch(context);
        }

        if (rightTex)
        {
            float scaleMul = m_isNextHovered ? 1.25f : 1.0f;
            Matrix rightWorld = Matrix::CreateScale(1.3f * scaleMul, 1.3f * scaleMul, 1.0f) *
                                Matrix::CreateTranslation(4.5f, -0.2f, 0.0f);
            m_effect->SetTexture(rightTex.Get());
            m_effect->SetWorld(rightWorld);
            m_effect->SetView(orthoView);
            m_effect->SetProjection(orthoProj);
            m_effect->SetDiffuseColor(m_isNextHovered ? Vector4(1.4f, 1.4f, 1.4f, 1.0f)
                                                      : Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            m_effect->Apply(context);
            context->IASetInputLayout(m_inputLayout.Get());
            DrawQuadWithPrimitiveBatch(context);
        }
    }

    // ★ 4. 2Dパーティクルの描画（2D UIやルールパネルの前面に描画）
    m_particleManager.Render(context, screenWidth, screenHeight, gameContext.commonStates);

    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->RSSetState(gameContext.commonStates.CullCounterClockwise());

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;

    // ガイドページのタイトルテキスト描画
    if (m_showRulePanel)
    {
        float textCenterX = screenWidth * 0.5f;
        float textCenterY = screenHeight * 0.08f;

        if (m_currentGuidePage == 0)
            debugRenderer.DrawText({textCenterX - 110.0f, textCenterY}, DirectX::Colors::White, L"STEP 1 : ROLL DICE");
        else if (m_currentGuidePage == 1)
            debugRenderer.DrawText({textCenterX - 95.0f, textCenterY}, DirectX::Colors::White, L"STEP 2 : MOVE PIECE");
        else if (m_currentGuidePage == 2)
            debugRenderer.DrawText({textCenterX - 95.0f, textCenterY}, DirectX::Colors::White,
                                   L"STEP 3 : CLASH / KILL");
    }

    debugRenderer.Render(context, &gameContext.commonStates);
}