#include "pch.h"
#include "GamePlaySceneBase.h"
#include "Framework/SceneSerializer.h"
#include <algorithm>
#include <fstream>
#include <Mouse.h>
#include <Keyboard.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void GamePlaySceneBase::TriggerTurnNotice(const std::wstring& text, float duration)
{
    m_turnNoticeText = text;
    m_turnNoticeMaxTime = duration;
    m_turnNoticeTimer = duration;
    AddLog(text.c_str());
}

void GamePlaySceneBase::ResetMoveState()
{
    m_hasMovedThisTurn = false;
    m_selectedX = -1;
    m_selectedY = -1;
    m_cursorX = -1;
    m_cursorY = -1;
}

void GamePlaySceneBase::OnEnter(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_field.Initialize(device, context, DiceBattleGame::BOARD_WIDTH, DiceBattleGame::BOARD_HEIGHT, 2.0f);
    m_gameLogic.Initialize(device, context);

    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");

    std::ifstream camFile("Resources/Data/camera_config.json");
    if (camFile.is_open())
    {
        try
        {
            nlohmann::json j;
            camFile >> j;
            if (j.contains("distance"))
                m_cameraDistance = j["distance"].get<float>();
            if (j.contains("pitch"))
                m_cameraPitch = j["pitch"].get<float>();
            if (j.contains("target"))
            {
                m_cameraTarget.x = j["target"]["x"].get<float>();
                m_cameraTarget.y = j["target"]["y"].get<float>();
                m_cameraTarget.z = j["target"]["z"].get<float>();
            }
        }
        catch (...)
        {
        }
    }

    m_dropAnimator.Clear();
    for (const auto& piece : m_gameLogic.GetPieces())
    {
        if (piece.isActive)
            m_dropAnimator.AddPiece(piece.diceInstance, piece.targetPos);
    }

    m_ai.SetThinkDelay(1.0f);

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionTexture>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetVertexColorEnabled(false);

    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count1.png", nullptr,
                                      m_diceCountTextures[1].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count2.png", nullptr,
                                      m_diceCountTextures[2].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count3.png", nullptr,
                                      m_diceCountTextures[3].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/RollDice.png", nullptr,
                                      m_rollDiceButtonTexture.ReleaseAndGetAddressOf());

    m_particleManager.Clear();
    m_isGameOver = false;
    m_gameOverSelectIndex = 0;
    m_winnerPlayer = PlayerType::Player1;
    m_hasMovedThisTurn = false;

    m_logManager.Initialize(device, context);
    m_previousTurn = m_gameLogic.GetCurrentTurn();
    m_gameLogic.SetRemainingSteps(0);

    // 派生クラスの初期化呼出
    OnModeEnter(gameContext);

    TriggerTurnNotice(L"あなたのターン！", 1.8f);
}

void GamePlaySceneBase::LoadSceneFromJson(GameContext& gameContext, const std::string& filePath)
{
    SceneData loadedData;
    if (!SceneSerializer::LoadFromJson(filePath, loadedData))
        return;

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
            continue;
        m_placedObjects.push_back(obj);

        if (objData.modelName == "Primitive_Cube")
        {
            obj.primitive = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
            m_loadedObjects.push_back(obj);
        }
        else
        {
            std::wstring fullPath(objData.modelName.begin(), objData.modelName.end());
            fullPath = L"Resources/Models/" + fullPath;
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

void GamePlaySceneBase::ExecuteMove(int fromX, int fromY, int toX, int toY)
{
    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    const auto* targetPiece = m_gameLogic.GetPieceAt(toX, toY);

    if (targetPiece && targetPiece->owner != currentTurn)
    {
        if (m_gameLogic.IsPlayerShielded(targetPiece->owner))
        {
            int remaining = m_gameLogic.GetRemainingSteps() - 1;
            if (remaining <= 0)
            {
                m_gameLogic.SetRemainingSteps(0);
                m_gameLogic.ForceEndTurn();
            }
            else
                m_gameLogic.SetRemainingSteps(remaining);

            TriggerTurnNotice(L"相手の「守護」により防がれた！", 1.2f);
            return;
        }
    }

    bool isEnemyDefeated = (targetPiece != nullptr && targetPiece->owner != currentTurn);

    if (m_gameLogic.MovePiece(fromX, fromY, toX, toY))
    {
        m_hasMovedThisTurn = true;
        m_selectedX = toX;
        m_selectedY = toY;
        m_cursorX = toX;
        m_cursorY = toY;

        std::wstring actor = (currentTurn == PlayerType::Player1) ? L"あなた" : L"CPU";
        if (isEnemyDefeated)
            m_logManager.AddLogFormat(L"{}がコマを移動し、あいてのこまを撃破！", actor.c_str());
        else
            m_logManager.AddLogFormat(L"{}がコマを ({}, {}) へ移動", actor.c_str(), toX, toY);
    }
}

void GamePlaySceneBase::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    auto& kb = gameContext.keyboardTracker;
    auto kbState = DirectX::Keyboard::Get().GetState();

    bool isCtrlPressed = kbState.IsKeyDown(DirectX::Keyboard::Keys::LeftControl) ||
                         kbState.IsKeyDown(DirectX::Keyboard::Keys::RightControl);

    if (isCtrlPressed)
    {
        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::W))
        {
            m_isGameOver = true;
            m_gameOverSelectIndex = 0;
            m_winnerPlayer = PlayerType::Player1;
            return;
        }
        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::M))
        {
            m_isGameOver = true;
            m_gameOverSelectIndex = 0;
            m_winnerPlayer = PlayerType::Player2;
            return;
        }

        float cameraSpeed = 20.0f * dt;
        if (kbState.IsKeyDown(DirectX::Keyboard::Keys::I))
            m_cameraDistance -= cameraSpeed;
        if (kbState.IsKeyDown(DirectX::Keyboard::Keys::K))
            m_cameraDistance += cameraSpeed;
        if (kbState.IsKeyDown(DirectX::Keyboard::Keys::U))
            m_cameraPitch += 25.0f * dt;
        if (kbState.IsKeyDown(DirectX::Keyboard::Keys::J))
            m_cameraPitch -= 25.0f * dt;
    }

    m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 60.0f);
    m_cameraPitch = std::clamp(m_cameraPitch, 15.0f, 85.0f);

    float radPitch = DirectX::XMConvertToRadians(m_cameraPitch);
    m_cameraEye.y = m_cameraTarget.y + m_cameraDistance * sinf(radPitch);
    m_cameraEye.z = m_cameraTarget.z - m_cameraDistance * cosf(radPitch);
    m_cameraEye.x = m_cameraTarget.x;

    if (m_turnNoticeTimer > 0.0f)
    {
        m_turnNoticeTimer -= dt;
        if (m_turnNoticeTimer < 0.0f)
            m_turnNoticeTimer = 0.0f;
    }

    m_particleManager.Update(dt);

    if (m_isGameOver)
    {
        auto mouseState = DirectX::Mouse::Get().GetState();
        static DirectX::Mouse::ButtonStateTracker mouseTracker;
        mouseTracker.Update(mouseState);

        m_isRetryHovered = (mouseState.x >= m_retryButtonRect.left && mouseState.x <= m_retryButtonRect.right &&
                            mouseState.y >= m_retryButtonRect.top && mouseState.y <= m_retryButtonRect.bottom);
        m_isTitleHovered = (mouseState.x >= m_titleButtonRect.left && mouseState.x <= m_titleButtonRect.right &&
                            mouseState.y >= m_titleButtonRect.top && mouseState.y <= m_titleButtonRect.bottom);

        if (m_isRetryHovered)
            m_gameOverSelectIndex = 0;
        if (m_isTitleHovered)
            m_gameOverSelectIndex = 1;

        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Up) || kb.IsKeyPressed(DirectX::Keyboard::Keys::W))
            m_gameOverSelectIndex = 0;
        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Down) || kb.IsKeyPressed(DirectX::Keyboard::Keys::S))
            m_gameOverSelectIndex = 1;

        bool isClicked = (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED) &&
                         (m_isRetryHovered || m_isTitleHovered);
        bool isEnterPressed =
            kb.IsKeyPressed(DirectX::Keyboard::Keys::Enter) || kb.IsKeyPressed(DirectX::Keyboard::Keys::Space);

        if (isClicked || isEnterPressed)
        {
            if (m_gameOverSelectIndex == 0)
                OnEnter(gameContext);
            else
                sceneController.RequestSwitch(SceneId::TitleScene);
        }
        return;
    }

    if (m_gameLogic.IsGameOver())
    {
        m_isGameOver = true;
        m_gameOverSelectIndex = 0;
        m_winnerPlayer = m_gameLogic.GetWinner();
        AddLog(m_winnerPlayer == PlayerType::Player1 ? L"ゲームエンド：あなたのしょうり！" : L"ゲームエンド：CPUのしょうり");
        return;
    }

    if (!m_dropAnimator.IsFinished())
    {
        m_dropAnimator.Update(dt);
        return;
    }

    // 派生クラス用のTurn更新前処理
    OnModeUpdate(dt, gameContext);

    m_gameLogic.Update(dt);
    m_field.Update(dt);

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    if (currentTurn != m_previousTurn)
    {
        m_previousTurn = currentTurn;
        m_hasMovedThisTurn = false;
        m_gameLogic.SetRemainingSteps(0);

        TriggerTurnNotice(currentTurn == PlayerType::Player1 ? L"あなたのターン！" : L"あいてのターン！", 1.5f);
    }

    if (m_gameLogic.GetCurrentTurn() == PlayerType::Player2)
    {
        m_gameLogic.SetPlayerShield(PlayerType::Player2, false);
        m_ai.Update(m_gameLogic, dt);
    }

    bool isPlayerTurn = (m_gameLogic.GetCurrentTurn() == PlayerType::Player1);
    bool canRoll = (isPlayerTurn && !m_gameLogic.IsWaitingForRoll() && !m_gameLogic.IsGameOver());

    auto mouseState = DirectX::Mouse::Get().GetState();
    static DirectX::Mouse::ButtonStateTracker mouseTracker;
    mouseTracker.Update(mouseState);

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    m_logManager.Update(mouseState, screenWidth, screenHeight);
    if (m_logManager.IsButtonClicked(mouseTracker))
    {
        m_logManager.ToggleWindow();
        m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y), 20);
    }

    Viewport viewport(0.0f, 0.0f, screenWidth, screenHeight);
    Matrix view = Matrix::CreateLookAt(m_cameraEye, m_cameraTarget, Vector3(0.0f, 1.0f, 0.0f));
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 1000.0f);

    if (isPlayerTurn)
    {
        m_gameLogic.SetPlayerShield(PlayerType::Player1, false);

        m_isRollButtonHovered = (mouseState.x >= m_rollButtonRect.left && mouseState.x <= m_rollButtonRect.right &&
                                 mouseState.y >= m_rollButtonRect.top && mouseState.y <= m_rollButtonRect.bottom);
        m_isUpHovered = (mouseState.x >= m_btnUpRect.left && mouseState.x <= m_btnUpRect.right &&
                         mouseState.y >= m_btnUpRect.top && mouseState.y <= m_btnUpRect.bottom);
        m_isDownHovered = (mouseState.x >= m_btnDownRect.left && mouseState.x <= m_btnDownRect.right &&
                           mouseState.y >= m_btnDownRect.top && mouseState.y <= m_btnDownRect.bottom);
        m_isLeftHovered = (mouseState.x >= m_btnLeftRect.left && mouseState.x <= m_btnLeftRect.right &&
                           mouseState.y >= m_btnLeftRect.top && mouseState.y <= m_btnLeftRect.bottom);
        m_isRightHovered = (mouseState.x >= m_btnRightRect.left && mouseState.x <= m_btnRightRect.right &&
                            mouseState.y >= m_btnRightRect.top && mouseState.y <= m_btnRightRect.bottom);

        if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !m_logManager.IsButtonHovered())
        {
            m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                                  25);
        }

        if (canRoll && (m_gameLogic.GetRemainingSteps() == 0) && m_isRollButtonHovered &&
            mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
        {
            m_gameLogic.StartTurn();
            ResetMoveState();
            AddLog(L"ダイスを振りました");
        }

        Vector3 nearPoint =
            viewport.Unproject(Vector3((float)mouseState.x, (float)mouseState.y, 0.0f), proj, view, Matrix::Identity);
        Vector3 farPoint =
            viewport.Unproject(Vector3((float)mouseState.x, (float)mouseState.y, 1.0f), proj, view, Matrix::Identity);
        Vector3 rayDir = farPoint - nearPoint;
        rayDir.Normalize();

        int hoveredGridX = -1, hoveredGridY = -1;
        if (std::abs(rayDir.y) > 0.0001f)
        {
            float t = -nearPoint.y / rayDir.y;
            Vector3 hitPoint = nearPoint + rayDir * t;
            float cellSize = m_field.GetCellSize();
            float startX = -(DiceBattleGame::BOARD_WIDTH * cellSize * 0.5f);
            float startZ = (DiceBattleGame::BOARD_HEIGHT * cellSize * 0.5f);

            hoveredGridX = static_cast<int>(std::floor((hitPoint.x - startX) / cellSize));
            hoveredGridY = static_cast<int>(std::floor((startZ - hitPoint.z) / cellSize));
        }

        if (IsModeSelectingTarget())
        {
            if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED &&
                !m_logManager.IsButtonHovered())
            {
                OnModeGridClicked(hoveredGridX, hoveredGridY);
            }
        }
        else
        {
            bool canControlPiece = !m_gameLogic.IsWaitingForRoll() && (m_gameLogic.GetRemainingSteps() > 0) &&
                                   !m_gameLogic.IsAnyPieceMoving();

            if (canControlPiece && !m_logManager.IsButtonHovered())
            {
                if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !m_isRollButtonHovered &&
                    !m_isUpHovered && !m_isDownHovered && !m_isLeftHovered && !m_isRightHovered)
                {
                    if (hoveredGridX >= 0 && hoveredGridX < DiceBattleGame::BOARD_WIDTH && hoveredGridY >= 0 &&
                        hoveredGridY < DiceBattleGame::BOARD_HEIGHT)
                    {
                        const auto* piece = m_gameLogic.GetPieceAt(hoveredGridX, hoveredGridY);
                        if (piece && piece->owner == PlayerType::Player1)
                        {
                            m_cursorX = hoveredGridX;
                            m_cursorY = hoveredGridY;
                            m_selectedX = hoveredGridX;
                            m_selectedY = hoveredGridY;
                        }
                    }
                }

                int moveDx = 0, moveDy = 0;
                if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
                {
                    if (m_isUpHovered)
                        moveDy = -1;
                    if (m_isDownHovered)
                        moveDy = 1;
                    if (m_isLeftHovered)
                        moveDx = 1;
                    if (m_isRightHovered)
                        moveDx = -1;
                }

                if (!isCtrlPressed)
                {
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Up) || kb.IsKeyPressed(DirectX::Keyboard::Keys::W))
                        moveDy = -1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Down) || kb.IsKeyPressed(DirectX::Keyboard::Keys::S))
                        moveDy = 1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Left) || kb.IsKeyPressed(DirectX::Keyboard::Keys::A))
                        moveDx = 1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Right) || kb.IsKeyPressed(DirectX::Keyboard::Keys::D))
                        moveDx = -1;
                }

                if (m_selectedX != -1 && (moveDx != 0 || moveDy != 0))
                    ExecuteMove(m_selectedX, m_selectedY, m_selectedX + moveDx, m_selectedY + moveDy);

                if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Escape) || m_gameLogic.GetRemainingSteps() <= 0)
                    ResetMoveState();
            }
        }
    }

    m_gameLogic.SetCursorPosition(m_cursorX, m_cursorY);

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    const wchar_t* playerStr = (m_gameLogic.GetCurrentTurn() == PlayerType::Player1) ? L"プレイヤー 1（あなた）"
                                                                                     : L"プレイヤー 2（CPU思考中...）";
    wchar_t turnStr[128];
    swprintf_s(turnStr, L"ターン: %s", playerStr);
    debugRenderer.DrawText({10.0f, 10.0f}, turnStr);
}

void GamePlaySceneBase::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Matrix view = Matrix::CreateLookAt(m_cameraEye, m_cameraTarget, Vector3(0.0f, 1.0f, 0.0f));
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 1000.0f);

    m_field.Render(context, gameContext.commonStates, view, proj, m_cursorX, m_cursorY, m_selectedX, m_selectedY,
                   false);
    m_gameLogic.Render(context, gameContext.commonStates, view, proj);

    for (const auto& obj : m_loadedObjects)
    {
        Matrix world = Matrix::CreateScale(obj.scale) * Matrix::CreateRotationX(XMConvertToRadians(obj.rotation.x)) *
                       Matrix::CreateRotationY(XMConvertToRadians(obj.rotation.y)) *
                       Matrix::CreateRotationZ(XMConvertToRadians(obj.rotation.z)) *
                       Matrix::CreateTranslation(obj.position);
        if (obj.model)
            obj.model->Draw(context, gameContext.commonStates, world, view, proj);
        else if (obj.primitive)
            obj.primitive->Draw(world, view, proj, Colors::White);
    }

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
    context->RSSetState(gameContext.commonStates.CullNone());

    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(Matrix::Identity);
    m_effect->SetProjection(orthoProj);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    // 2Dエフェクト準備
    void const* colorByteCode;
    size_t colorByteCodeLength;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(VertexPositionColor::InputElements,
                                                                  VertexPositionColor::InputElementCount, colorByteCode,
                                                                  colorByteCodeLength, colorInputLayout.GetAddressOf());

    void const* shaderByteCode;
    size_t byteCodeLength;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(
        VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount, shaderByteCode, byteCodeLength,
        inputLayout.GetAddressOf());

    // 派生クラスのUI・2D描画
    OnModeRender2D(context, screenWidth, screenHeight, gameContext);

    m_logManager.Render(context, gameContext.commonStates, gameContext.debugRenderer, m_primitiveBatch.get(),
                        m_colorPrimitiveBatch.get(), m_effect.get(), m_colorEffect.get(), screenWidth, screenHeight);

    // 矢印ボタン描画
    bool canControlPiece = (m_gameLogic.GetCurrentTurn() == PlayerType::Player1) && !m_gameLogic.IsWaitingForRoll() &&
                           (m_gameLogic.GetRemainingSteps() > 0) && !m_gameLogic.IsAnyPieceMoving();
    if (canControlPiece)
    {
        context->IASetInputLayout(colorInputLayout.Get());
        m_colorEffect->Apply(context);
        m_colorPrimitiveBatch->Begin();

        float btnSize = 45.0f, spacing = 12.0f;
        float arrowCenterX = screenWidth - 160.0f, arrowCenterY = screenHeight - 180.0f;
        float offset = btnSize + spacing;

        m_btnUpRect = {static_cast<LONG>(arrowCenterX - btnSize * 0.5f),
                       static_cast<LONG>(arrowCenterY - btnSize * 0.5f - offset),
                       static_cast<LONG>(arrowCenterX + btnSize * 0.5f),
                       static_cast<LONG>(arrowCenterY + btnSize * 0.5f - offset)};
        m_btnDownRect = {static_cast<LONG>(arrowCenterX - btnSize * 0.5f),
                         static_cast<LONG>(arrowCenterY - btnSize * 0.5f + offset),
                         static_cast<LONG>(arrowCenterX + btnSize * 0.5f),
                         static_cast<LONG>(arrowCenterY + btnSize * 0.5f + offset)};
        m_btnLeftRect = {static_cast<LONG>(arrowCenterX - btnSize * 0.5f - offset),
                         static_cast<LONG>(arrowCenterY - btnSize * 0.5f),
                         static_cast<LONG>(arrowCenterX + btnSize * 0.5f - offset),
                         static_cast<LONG>(arrowCenterY + btnSize * 0.5f)};
        m_btnRightRect = {static_cast<LONG>(arrowCenterX - btnSize * 0.5f + offset),
                          static_cast<LONG>(arrowCenterY - btnSize * 0.5f),
                          static_cast<LONG>(arrowCenterX + btnSize * 0.5f + offset),
                          static_cast<LONG>(arrowCenterY + btnSize * 0.5f)};

        bool isSelected = (m_selectedX != -1);
        float alpha = isSelected ? 0.85f : 0.3f;

        auto DrawButtonBg = [&](const RECT& rect, bool isHovered, Vector4 baseCol)
        {
            Vector4 col = isHovered && isSelected ? Vector4(1.0f, 0.85f, 0.2f, 0.95f) : baseCol;
            col.w *= alpha;
            VertexPositionColor v0(Vector3((float)rect.left, (float)rect.top, 0), col);
            VertexPositionColor v1(Vector3((float)rect.right, (float)rect.top, 0), col);
            VertexPositionColor v2(Vector3((float)rect.left, (float)rect.bottom, 0), col);
            VertexPositionColor v3(Vector3((float)rect.right, (float)rect.bottom, 0), col);
            m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
            m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
        };

        DrawButtonBg(m_btnUpRect, m_isUpHovered, Vector4(0.1f, 0.5f, 0.9f, 0.8f));
        DrawButtonBg(m_btnDownRect, m_isDownHovered, Vector4(0.1f, 0.5f, 0.9f, 0.8f));
        DrawButtonBg(m_btnLeftRect, m_isLeftHovered, Vector4(0.1f, 0.5f, 0.9f, 0.8f));
        DrawButtonBg(m_btnRightRect, m_isRightHovered, Vector4(0.1f, 0.5f, 0.9f, 0.8f));
        m_colorPrimitiveBatch->End();
    }

    // ダイスボタン
    context->IASetInputLayout(inputLayout.Get());
    bool canRoll = (m_gameLogic.GetCurrentTurn() == PlayerType::Player1 && !m_gameLogic.IsWaitingForRoll() &&
                    !m_gameLogic.IsGameOver() && m_gameLogic.GetRemainingSteps() == 0);
    if (canRoll && m_rollDiceButtonTexture)
    {
        float baseWidth = 220.0f, baseHeight = 120.0f;
        float scale = m_isRollButtonHovered ? 1.08f : 1.0f;
        float bWidth = baseWidth * scale, bHeight = baseHeight * scale;
        float btnX = (screenWidth - baseWidth - 20.0f) - (bWidth - baseWidth) * 0.5f;
        float btnY = (screenHeight - baseHeight - 20.0f) - (bHeight - baseHeight) * 0.5f;

        m_rollButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY), static_cast<LONG>(btnX + bWidth),
                            static_cast<LONG>(btnY + bHeight)};
        m_effect->SetTexture(m_rollDiceButtonTexture.Get());
        m_effect->Apply(context);
        m_primitiveBatch->Begin();
        VertexPositionTexture v0(Vector3(btnX, btnY, 0.0f), Vector2(0.0f, 0.0f));
        VertexPositionTexture v1(Vector3(btnX + bWidth, btnY, 0.0f), Vector2(1.0f, 0.0f));
        VertexPositionTexture v2(Vector3(btnX, btnY + bHeight, 0.0f), Vector2(0.0f, 1.0f));
        VertexPositionTexture v3(Vector3(btnX + bWidth, btnY + bHeight, 0.0f), Vector2(1.0f, 1.0f));
        m_primitiveBatch->DrawTriangle(v0, v1, v2);
        m_primitiveBatch->DrawTriangle(v2, v1, v3);
        m_primitiveBatch->End();
    }

    // パーティクル
    context->IASetInputLayout(colorInputLayout.Get());
    m_particleManager.Render(context, m_colorPrimitiveBatch.get(), m_colorEffect.get());

    // ターン告知
    if (m_turnNoticeTimer > 0.0f && !m_turnNoticeText.empty())
    {
        float textX = (screenWidth * 0.5f) - (static_cast<float>(m_turnNoticeText.length()) * 8.0f);
        float textY = screenHeight * 0.35f;

        m_colorEffect->Apply(context);
        m_colorPrimitiveBatch->Begin();
        Vector4 bgCol(0.0f, 0.0f, 0.0f, 0.8f);
        VertexPositionColor bgV0(Vector3(0.0f, textY - 10.0f, 0.0f), bgCol);
        VertexPositionColor bgV1(Vector3(screenWidth, textY - 10.0f, 0.0f), bgCol);
        VertexPositionColor bgV2(Vector3(0.0f, textY + 30.0f, 0.0f), bgCol);
        VertexPositionColor bgV3(Vector3(screenWidth, textY + 30.0f, 0.0f), bgCol);
        m_colorPrimitiveBatch->DrawTriangle(bgV0, bgV1, bgV2);
        m_colorPrimitiveBatch->DrawTriangle(bgV2, bgV1, bgV3);
        m_colorPrimitiveBatch->End();

        gameContext.debugRenderer.DrawText({textX, textY}, m_turnNoticeText.c_str());
    }

    // リザルトダイアログ
    if (m_isGameOver)
    {
        m_colorEffect->Apply(context);
        m_colorPrimitiveBatch->Begin();
        VertexPositionColor bg0(Vector3(0.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
        VertexPositionColor bg1(Vector3(screenWidth, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
        VertexPositionColor bg2(Vector3(0.0f, screenHeight, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
        VertexPositionColor bg3(Vector3(screenWidth, screenHeight, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
        m_colorPrimitiveBatch->DrawTriangle(bg0, bg1, bg2);
        m_colorPrimitiveBatch->DrawTriangle(bg2, bg1, bg3);

        float dialogW = 400.0f, dialogH = 260.0f;
        float dialogX = (screenWidth - dialogW) * 0.5f, dialogY = (screenHeight - dialogH) * 0.5f;

        VertexPositionColor dlg0(Vector3(dialogX, dialogY, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
        VertexPositionColor dlg1(Vector3(dialogX + dialogW, dialogY, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
        VertexPositionColor dlg2(Vector3(dialogX, dialogY + dialogH, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
        VertexPositionColor dlg3(Vector3(dialogX + dialogW, dialogY + dialogH, 0.0f),
                                 Vector4(0.12f, 0.12f, 0.18f, 0.95f));
        m_colorPrimitiveBatch->DrawTriangle(dlg0, dlg1, dlg2);
        m_colorPrimitiveBatch->DrawTriangle(dlg2, dlg1, dlg3);

        float btnW = 220.0f, btnH = 45.0f;
        float btnX = dialogX + (dialogW - btnW) * 0.5f;
        float btnY1 = dialogY + 110.0f, btnY2 = dialogY + 175.0f;

        m_retryButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY1), static_cast<LONG>(btnX + btnW),
                             static_cast<LONG>(btnY1 + btnH)};
        m_titleButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY2), static_cast<LONG>(btnX + btnW),
                             static_cast<LONG>(btnY2 + btnH)};

        Vector4 col1 =
            (m_gameOverSelectIndex == 0) ? Vector4(0.9f, 0.7f, 0.1f, 0.9f) : Vector4(0.3f, 0.35f, 0.45f, 0.8f);
        Vector4 col2 =
            (m_gameOverSelectIndex == 1) ? Vector4(0.9f, 0.7f, 0.1f, 0.9f) : Vector4(0.3f, 0.35f, 0.45f, 0.8f);

        auto DrawBox = [&](float x, float y, float w, float h, Vector4 col)
        {
            VertexPositionColor v0(Vector3(x, y, 0.0f), col);
            VertexPositionColor v1(Vector3(x + w, y, 0.0f), col);
            VertexPositionColor v2(Vector3(x, y + h, 0.0f), col);
            VertexPositionColor v3(Vector3(x + w, y + h, 0.0f), col);
            m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
            m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
        };

        DrawBox(btnX, btnY1, btnW, btnH, col1);
        DrawBox(btnX, btnY2, btnW, btnH, col2);
        m_colorPrimitiveBatch->End();

        const wchar_t* winStr = (m_winnerPlayer == PlayerType::Player1) ? L"YOU WIN!" : L"YOU LOSE...";
        gameContext.debugRenderer.DrawText({dialogX + 130.0f, dialogY + 30.0f}, winStr);
        gameContext.debugRenderer.DrawText({btnX + 65.0f, btnY1 + 12.0f}, L"再戦");
        gameContext.debugRenderer.DrawText({btnX + 55.0f, btnY2 + 12.0f}, L"タイトルへ");
    }
}