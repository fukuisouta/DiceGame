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

Dice* GamePlaySceneBase::GetTargetDiceInstance()
{
    if (m_selectedX >= 0 && m_selectedY >= 0)
    {
        const auto* piece = m_gameLogic.GetPieceAt(m_selectedX, m_selectedY);
        if (piece && piece->isActive && piece->diceInstance)
            return piece->diceInstance.get();
    }

    if (m_cursorX >= 0 && m_cursorY >= 0)
    {
        const auto* piece = m_gameLogic.GetPieceAt(m_cursorX, m_cursorY);
        if (piece && piece->isActive && piece->diceInstance)
            return piece->diceInstance.get();
    }

    const auto& pieces = m_gameLogic.GetPieces();
    for (const auto& piece : pieces)
    {
        if (piece.isActive && piece.diceInstance)
            return piece.diceInstance.get();
    }

    return nullptr;
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

    m_selectionMarker = DirectX::GeometricPrimitive::CreateCone(context, 0.8f, 1.0f, 12, false);
    m_markerAnimTimer = 0.0f;

    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count1.png", nullptr,
                                      m_diceCountTextures[1].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count2.png", nullptr,
                                      m_diceCountTextures[2].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Count3.png", nullptr,
                                      m_diceCountTextures[3].ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/RollDice.png", nullptr,
                                      m_rollDiceButtonTexture.ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/DiceCheck.png", nullptr,
                                      m_diceCheckButtonTexture.ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/LOG.png", nullptr,
                                      m_logButtonTexture.ReleaseAndGetAddressOf());

    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Flont.png", nullptr,
                                      m_arrowUpTexture.ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Back.png", nullptr,
                                      m_arrowDownTexture.ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Left.png", nullptr,
                                      m_arrowLeftTexture.ReleaseAndGetAddressOf());
    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/Right.png", nullptr,
                                      m_arrowRightTexture.ReleaseAndGetAddressOf());

    m_particleManager.Clear();
    m_isGameOver = false;
    m_gameOverSelectIndex = 0;
    m_winnerPlayer = PlayerType::Player1;
    m_hasMovedThisTurn = false;

    m_logManager.Initialize(device, context);
    m_previousTurn = m_gameLogic.GetCurrentTurn();
    m_gameLogic.SetRemainingSteps(0);

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    m_diceViewer.Initialize(screenWidth / screenHeight);

    // ★【修正】ダイスを振り直さず、受信した出目を即座に確定表示する
    gameContext.networkManager.SetOnDiceResult(
        [this, &gameContext](int steps)
        {
            bool isClient = (gameContext.networkManager.IsConnected() &&
                             gameContext.networkManager.GetMode() == NetworkManager::Mode::Client);
            PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

            // 自ターンの返送パケット（ループバック）は無視
            if (m_gameLogic.GetCurrentTurn() == myPlayerType)
            {
                return;
            }

            // 相手ターンの場合は、ダイスを投げ直さずに即座に出目を確定セット
            m_gameLogic.SetDiceResultImmediately(steps);
            ResetMoveState();
            m_logManager.AddLogFormat(L"あいてがダイスを振り【%d】が出ました！", steps);
        });

    gameContext.networkManager.SetOnMovePiece([this, &gameContext](int fX, int fY, int tX, int tY)
                                              { ExecuteMove(fX, fY, tX, tY, true, &gameContext); });

    OnModeEnter(gameContext);

    TriggerTurnNotice(L"ゲーム開始", 1.8f);
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

void GamePlaySceneBase::ExecuteMove(int fromX, int fromY, int toX, int toY, bool isNetworkReceived,
                                    GameContext* gameContext)
{
    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();

    const auto* attackerPiece = m_gameLogic.GetPieceAt(fromX, fromY);
    const auto* targetPiece = m_gameLogic.GetPieceAt(toX, toY);

    int atkFace = m_gameLogic.GetPieceTopFace(attackerPiece);
    int defFace = m_gameLogic.GetPieceTopFace(targetPiece);

    bool isEnemyDefeated = (targetPiece != nullptr && targetPiece->owner != currentTurn);

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

            TriggerTurnNotice(L"相手の「守護」により防がれた", 1.2f);
            return;
        }
    }

    if (m_gameLogic.MovePiece(fromX, fromY, toX, toY))
    {
        m_hasMovedThisTurn = true;
        m_selectedX = toX;
        m_selectedY = toY;
        m_cursorX = toX;
        m_cursorY = toY;

        bool isClient = (gameContext && gameContext->networkManager.IsConnected() &&
                         gameContext->networkManager.GetMode() == NetworkManager::Mode::Client);
        PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

        const wchar_t* actor = (currentTurn == myPlayerType) ? L"あなた" : L"あいて";
        const wchar_t* targetName = (currentTurn == myPlayerType) ? L"あいて" : L"あなた";

        if (isEnemyDefeated)
        {
            m_logManager.AddLogFormat(L"%s[目:%d] が %s[目:%d] を撃破！", actor, atkFace, targetName, defFace);
        }
        else
        {
            m_logManager.AddLogFormat(L"%sがコマ[目:%d] を (%d, %d) へ移動", actor, atkFace, toX, toY);
        }

        if (!isNetworkReceived && gameContext && gameContext->networkManager.IsConnected())
        {
            gameContext->networkManager.SendMovePiece(fromX, fromY, toX, toY);
        }
    }
}

void GamePlaySceneBase::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    auto& kb = gameContext.keyboardTracker;
    auto kbState = DirectX::Keyboard::Get().GetState();

    auto mouseState = DirectX::Mouse::Get().GetState();
    static DirectX::Mouse::ButtonStateTracker mouseTracker;
    mouseTracker.Update(mouseState);

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    gameContext.networkManager.DispatchPackets();

    if (m_diceViewer.IsOpen())
    {
        m_diceViewer.SetAspectRatio(screenWidth / screenHeight);
        m_diceViewer.Update(mouseState, dt);

        m_isDiceCheckCloseHovered =
            (mouseState.x >= m_btnDiceCheckCloseRect.left && mouseState.x <= m_btnDiceCheckCloseRect.right &&
             mouseState.y >= m_btnDiceCheckCloseRect.top && mouseState.y <= m_btnDiceCheckCloseRect.bottom);
        m_isDiceCheckResetHovered =
            (mouseState.x >= m_btnDiceCheckResetRect.left && mouseState.x <= m_btnDiceCheckResetRect.right &&
             mouseState.y >= m_btnDiceCheckResetRect.top && mouseState.y <= m_btnDiceCheckResetRect.bottom);

        if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
        {
            if (m_isDiceCheckCloseHovered)
            {
                m_diceViewer.Close();
                return;
            }
            if (m_isDiceCheckResetHovered)
            {
                Dice* targetDice = GetTargetDiceInstance();
                if (targetDice)
                    m_diceViewer.ResetCameraWithRotation(targetDice->GetRotation());
                else
                    m_diceViewer.ResetCamera();
            }
        }

        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Escape))
        {
            m_diceViewer.Close();
            return;
        }

        return;
    }

    m_isDiceCheckBtnHovered = (mouseState.x >= m_btnDiceCheckRect.left && mouseState.x <= m_btnDiceCheckRect.right &&
                               mouseState.y >= m_btnDiceCheckRect.top && mouseState.y <= m_btnDiceCheckRect.bottom);

    if (m_isDiceCheckBtnHovered && mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
    {
        m_diceViewer.Open();

        Dice* targetDice = GetTargetDiceInstance();
        if (targetDice)
            m_diceViewer.ResetCameraWithRotation(targetDice->GetRotation());
        else
            m_diceViewer.ResetCamera();

        return;
    }

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

    bool isClient = (gameContext.networkManager.IsConnected() &&
                     gameContext.networkManager.GetMode() == NetworkManager::Mode::Client);

    if (isClient)
    {
        m_cameraEye.y = m_cameraTarget.y + m_cameraDistance * sinf(radPitch);
        m_cameraEye.z = m_cameraTarget.z + m_cameraDistance * cosf(radPitch);
        m_cameraEye.x = m_cameraTarget.x;
    }
    else
    {
        m_cameraEye.y = m_cameraTarget.y + m_cameraDistance * sinf(radPitch);
        m_cameraEye.z = m_cameraTarget.z - m_cameraDistance * cosf(radPitch);
        m_cameraEye.x = m_cameraTarget.x;
    }

    if (m_turnNoticeTimer > 0.0f)
    {
        m_turnNoticeTimer -= dt;
        if (m_turnNoticeTimer < 0.0f)
            m_turnNoticeTimer = 0.0f;
    }

    m_particleManager.Update(dt);

    if (m_isGameOver)
    {
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
        m_isDraw = m_gameLogic.IsDraw();
        m_gameOverSelectIndex = 0;
        m_winnerPlayer = m_gameLogic.GetWinner();

        if (m_isDraw)
        {
            AddLog(L"ゲームエンド：引き分け（DRAW）です");
        }
        else
        {
            AddLog(m_winnerPlayer == PlayerType::Player1 ? L"ゲームエンド：あなたのしょうり"
                                                         : L"ゲームエンド：あいてのしょうり");
        }
        return;
    }

    if (!m_dropAnimator.IsFinished())
    {
        m_dropAnimator.Update(dt);
        return;
    }

    OnModeUpdate(dt, gameContext);

    m_gameLogic.Update(dt);
    m_field.Update(dt);

    if (m_gameLogic.GetJustLandedSteps() > 0)
    {
        int steps = m_gameLogic.GetJustLandedSteps();
        if (steps > 0)
        {
            PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
            PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

            if (currentTurn == myPlayerType)
            {
                m_logManager.AddLogFormat(L"あなたがダイスを振り【%d】が出ました！", steps);

                if (gameContext.networkManager.IsConnected())
                {
                    gameContext.networkManager.SendDiceResult(steps);
                }
            }
        }
        m_gameLogic.ClearJustLandedSteps();
    }

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    if (currentTurn != m_previousTurn)
    {
        m_previousTurn = currentTurn;
        m_hasMovedThisTurn = false;
        m_gameLogic.SetRemainingSteps(0);

        TriggerTurnNotice(currentTurn == PlayerType::Player1 ? L"あなたのターン" : L"あいてのターン", 1.5f);
    }

    if (!gameContext.networkManager.IsConnected() && m_gameLogic.GetCurrentTurn() == PlayerType::Player2)
    {
        m_gameLogic.SetPlayerShield(PlayerType::Player2, false);
        int stepsBefore = m_gameLogic.GetRemainingSteps();

        m_ai.Update(m_gameLogic, dt);

        if (m_gameLogic.GetRemainingSteps() < stepsBefore && stepsBefore > 0)
        {
            AddLog(L"あいて(CPU)がコマを移動しました");
        }
    }

    bool canRoll = (!m_gameLogic.IsWaitingForRoll() && !m_gameLogic.IsGameOver());

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

    PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

    if (m_gameLogic.GetCurrentTurn() == myPlayerType)
    {
        m_gameLogic.SetPlayerShield(myPlayerType, false);

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

        if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !m_logManager.IsButtonHovered() &&
            !m_isDiceCheckBtnHovered)
        {
            m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                                  25);
        }

        if (canRoll && (m_gameLogic.GetRemainingSteps() == 0) && m_isRollButtonHovered &&
            mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
        {
            m_gameLogic.StartTurn();
            ResetMoveState();
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
                !m_logManager.IsButtonHovered() && !m_isDiceCheckBtnHovered)
            {
                OnModeGridClicked(hoveredGridX, hoveredGridY);
            }
        }
        else
        {
            bool canControlPiece = !m_gameLogic.IsWaitingForRoll() && (m_gameLogic.GetRemainingSteps() > 0) &&
                                   !m_gameLogic.IsAnyPieceMoving();

            if (canControlPiece && !m_logManager.IsButtonHovered() && !m_isDiceCheckBtnHovered)
            {
                if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !m_isRollButtonHovered &&
                    !m_isUpHovered && !m_isDownHovered && !m_isLeftHovered && !m_isRightHovered)
                {
                    if (hoveredGridX >= 0 && hoveredGridX < DiceBattleGame::BOARD_WIDTH && hoveredGridY >= 0 &&
                        hoveredGridY < DiceBattleGame::BOARD_HEIGHT)
                    {
                        const auto* piece = m_gameLogic.GetPieceAt(hoveredGridX, hoveredGridY);
                        if (piece && piece->owner == myPlayerType)
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
                        moveDy = isClient ? 1 : -1;
                    if (m_isDownHovered)
                        moveDy = isClient ? -1 : 1;
                    if (m_isLeftHovered)
                        moveDx = isClient ? -1 : 1;
                    if (m_isRightHovered)
                        moveDx = isClient ? 1 : -1;
                }

                if (!isCtrlPressed)
                {
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Up) || kb.IsKeyPressed(DirectX::Keyboard::Keys::W))
                        moveDy = isClient ? 1 : -1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Down) || kb.IsKeyPressed(DirectX::Keyboard::Keys::S))
                        moveDy = isClient ? -1 : 1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Left) || kb.IsKeyPressed(DirectX::Keyboard::Keys::A))
                        moveDx = isClient ? -1 : 1;
                    if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Right) || kb.IsKeyPressed(DirectX::Keyboard::Keys::D))
                        moveDx = isClient ? 1 : -1;
                }

                if (m_selectedX != -1 && (moveDx != 0 || moveDy != 0))
                    ExecuteMove(m_selectedX, m_selectedY, m_selectedX + moveDx, m_selectedY + moveDy, false,
                                &gameContext);

                if (kb.IsKeyPressed(DirectX::Keyboard::Keys::Escape) || m_gameLogic.GetRemainingSteps() <= 0)
                    ResetMoveState();
            }
        }
    }

    m_gameLogic.SetCursorPosition(m_cursorX, m_cursorY);
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

    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    m_markerAnimTimer += dt;

    bool isClient = (gameContext.networkManager.IsConnected() &&
                     gameContext.networkManager.GetMode() == NetworkManager::Mode::Client);
    PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

    if (m_selectedX >= 0 && m_selectedY >= 0 && !m_diceViewer.IsOpen())
    {
        const auto* piece = m_gameLogic.GetPieceAt(m_selectedX, m_selectedY);
        if (piece && piece->isActive && piece->owner == myPlayerType && m_selectionMarker)
        {
            Vector3 basePos = piece->targetPos;

            float hoverHeight = 2.4f;
            float wave = sinf(m_markerAnimTimer * 6.0f) * 0.15f;
            Vector3 markerPos = basePos + Vector3(0.0f, hoverHeight + wave, 0.0f);

            Matrix markerWorld = Matrix::CreateRotationZ(XM_PI) * Matrix::CreateRotationY(m_markerAnimTimer * 2.5f) *
                                 Matrix::CreateTranslation(markerPos);

            m_selectionMarker->Draw(markerWorld, view, proj, Colors::Gold);
        }
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

    OnModeRender2D(context, screenWidth, screenHeight, gameContext);

    m_logManager.Render(context, gameContext.commonStates, gameContext.debugRenderer, m_colorPrimitiveBatch.get(),
                        m_colorEffect.get(), screenWidth, screenHeight);

    if (!m_diceViewer.IsOpen() && !m_isGameOver)
    {
        float topBtnW = 180.0f;
        float topBtnH = 100.0f;
        float margin = 20.0f;

        m_btnDiceCheckRect = {static_cast<LONG>(margin), static_cast<LONG>(margin), static_cast<LONG>(margin + topBtnW),
                              static_cast<LONG>(margin + topBtnH)};

        RECT logRect = {static_cast<LONG>(screenWidth - topBtnW - margin), static_cast<LONG>(margin),
                        static_cast<LONG>(screenWidth - margin), static_cast<LONG>(margin + topBtnH)};
        m_logManager.SetButtonRect(logRect);

        auto DrawTextureButton = [&](ID3D11ShaderResourceView* tex, const RECT& rect, bool isHovered)
        {
            if (!tex)
                return;

            float baseW = static_cast<float>(rect.right - rect.left);
            float baseH = static_cast<float>(rect.bottom - rect.top);
            float scale = isHovered ? 1.05f : 1.0f;

            float bWidth = baseW * scale;
            float bHeight = baseH * scale;

            float btnX = static_cast<float>(rect.left) - (bWidth - baseW) * 0.5f;
            float btnY = static_cast<float>(rect.top) - (bHeight - baseH) * 0.5f;

            context->IASetInputLayout(inputLayout.Get());
            m_effect->SetTexture(tex);
            m_effect->Apply(context);

            m_primitiveBatch->Begin();
            VertexPositionTexture v0(Vector3(btnX, btnY, 0.0f), Vector2(0.0f, 0.0f));
            VertexPositionTexture v1(Vector3(btnX + bWidth, btnY, 0.0f), Vector2(1.0f, 0.0f));
            VertexPositionTexture v2(Vector3(btnX, btnY + bHeight, 0.0f), Vector2(0.0f, 1.0f));
            VertexPositionTexture v3(Vector3(btnX + bWidth, btnY + bHeight, 0.0f), Vector2(1.0f, 1.0f));
            m_primitiveBatch->DrawTriangle(v0, v1, v2);
            m_primitiveBatch->DrawTriangle(v2, v1, v3);
            m_primitiveBatch->End();
        };

        DrawTextureButton(m_diceCheckButtonTexture.Get(), m_btnDiceCheckRect, m_isDiceCheckBtnHovered);
        DrawTextureButton(m_logButtonTexture.Get(), logRect, m_logManager.IsButtonHovered());
    }

    bool canControlPiece = (m_gameLogic.GetCurrentTurn() == myPlayerType) && !m_gameLogic.IsWaitingForRoll() &&
                           (m_gameLogic.GetRemainingSteps() > 0) && !m_gameLogic.IsAnyPieceMoving();
    if (canControlPiece && !m_diceViewer.IsOpen())
    {
        float arrowCenterX = screenWidth - 160.0f;
        float arrowCenterY = screenHeight - 180.0f;
        float spacing = 15.0f;

        bool isSelected = (m_selectedX != -1);

        auto DrawArrowButton = [&](ID3D11ShaderResourceView* tex, bool isHovered, float baseW, float baseH, float cx,
                                   float cy, RECT& outRect)
        {
            float scale = (isHovered && isSelected) ? 1.1f : 1.0f;
            float btnW = baseW * scale;
            float btnH = baseH * scale;

            float x = cx - btnW * 0.5f;
            float y = cy - btnH * 0.5f;

            outRect = {static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + btnW),
                       static_cast<LONG>(y + btnH)};

            if (tex)
            {
                context->IASetInputLayout(inputLayout.Get());
                m_effect->SetTexture(tex);
                m_effect->Apply(context);

                m_primitiveBatch->Begin();
                VertexPositionTexture v0(Vector3(x, y, 0.0f), Vector2(0.0f, 0.0f));
                VertexPositionTexture v1(Vector3(x + btnW, y, 0.0f), Vector2(1.0f, 0.0f));
                VertexPositionTexture v2(Vector3(x, y + btnH, 0.0f), Vector2(0.0f, 1.0f));
                VertexPositionTexture v3(Vector3(x + btnW, y + btnH, 0.0f), Vector2(1.0f, 1.0f));
                m_primitiveBatch->DrawTriangle(v0, v1, v2);
                m_primitiveBatch->DrawTriangle(v2, v1, v3);
                m_primitiveBatch->End();
            }
        };

        float buttonSize = 85.0f;
        float offsetDistance = buttonSize * 0.85f + spacing;

        DrawArrowButton(m_arrowUpTexture.Get(), m_isUpHovered, buttonSize, buttonSize, arrowCenterX,
                        arrowCenterY - offsetDistance, m_btnUpRect);

        DrawArrowButton(m_arrowDownTexture.Get(), m_isDownHovered, buttonSize, buttonSize, arrowCenterX,
                        arrowCenterY + offsetDistance, m_btnDownRect);

        DrawArrowButton(m_arrowLeftTexture.Get(), m_isLeftHovered, buttonSize, buttonSize,
                        arrowCenterX - offsetDistance, arrowCenterY, m_btnLeftRect);

        DrawArrowButton(m_arrowRightTexture.Get(), m_isRightHovered, buttonSize, buttonSize,
                        arrowCenterX + offsetDistance, arrowCenterY, m_btnRightRect);
    }

    context->IASetInputLayout(inputLayout.Get());
    bool canRoll = (m_gameLogic.GetCurrentTurn() == myPlayerType && !m_gameLogic.IsWaitingForRoll() &&
                    !m_gameLogic.IsGameOver() && m_gameLogic.GetRemainingSteps() == 0);
    if (canRoll && m_rollDiceButtonTexture && !m_diceViewer.IsOpen())
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

    context->IASetInputLayout(colorInputLayout.Get());
    m_particleManager.Render(context, m_colorPrimitiveBatch.get(), m_colorEffect.get());

    if (m_turnNoticeTimer > 0.0f && !m_turnNoticeText.empty() && !m_diceViewer.IsOpen())
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

        gameContext.debugRenderer.DrawText({textX, textY}, DirectX::Colors::White, m_turnNoticeText.c_str());
    }

    if (m_diceViewer.IsOpen())
    {
        context->IASetInputLayout(colorInputLayout.Get());
        m_colorEffect->Apply(context);
        m_colorPrimitiveBatch->Begin();
        Vector4 bgCol(0.0f, 0.0f, 0.0f, 0.75f);
        VertexPositionColor bg0(Vector3(0.0f, 0.0f, 0.0f), bgCol);
        VertexPositionColor bg1(Vector3(screenWidth, 0.0f, 0.0f), bgCol);
        VertexPositionColor bg2(Vector3(0.0f, screenHeight, 0.0f), bgCol);
        VertexPositionColor bg3(Vector3(screenWidth, screenHeight, 0.0f), bgCol);
        m_colorPrimitiveBatch->DrawTriangle(bg0, bg1, bg2);
        m_colorPrimitiveBatch->DrawTriangle(bg2, bg1, bg3);

        m_btnDiceCheckResetRect = {static_cast<LONG>(screenWidth - 270), 30, static_cast<LONG>(screenWidth - 150), 70};
        m_btnDiceCheckCloseRect = {static_cast<LONG>(screenWidth - 130), 30, static_cast<LONG>(screenWidth - 10), 70};

        Vector4 resetCol =
            m_isDiceCheckResetHovered ? Vector4(0.8f, 0.8f, 0.2f, 0.9f) : Vector4(0.4f, 0.4f, 0.4f, 0.8f);
        Vector4 closeCol =
            m_isDiceCheckCloseHovered ? Vector4(0.9f, 0.3f, 0.3f, 0.9f) : Vector4(0.6f, 0.2f, 0.2f, 0.8f);

        auto DrawBox = [&](const RECT& rect, Vector4 col)
        {
            VertexPositionColor v0(Vector3((float)rect.left, (float)rect.top, 0), col);
            VertexPositionColor v1(Vector3((float)rect.right, (float)rect.top, 0), col);
            VertexPositionColor v2(Vector3((float)rect.left, (float)rect.bottom, 0), col);
            VertexPositionColor v3(Vector3((float)rect.right, (float)rect.bottom, 0), col);
            m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
            m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
        };

        DrawBox(m_btnDiceCheckResetRect, resetCol);
        DrawBox(m_btnDiceCheckCloseRect, closeCol);
        m_colorPrimitiveBatch->End();

        gameContext.debugRenderer.DrawText({30.0f, 30.0f}, DirectX::Colors::White, L"【ダイスチェックモード】");
        gameContext.debugRenderer.DrawText({30.0f, 60.0f}, DirectX::Colors::White,
                                           L"ドラッグ: 自由回転 | ホイール: 拡大・縮小");
        gameContext.debugRenderer.DrawText({static_cast<float>(screenWidth - 250), 40.0f}, DirectX::Colors::White,
                                           L"リセット");
        gameContext.debugRenderer.DrawText({static_cast<float>(screenWidth - 100), 40.0f}, DirectX::Colors::White,
                                           L"閉じる");

        context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

        Matrix viewerView = m_diceViewer.GetViewMatrix();
        Matrix viewerProj = m_diceViewer.GetProjectionMatrix();
        Matrix viewerWorld = m_diceViewer.GetWorldMatrix();

        Dice* targetDice = GetTargetDiceInstance();
        if (targetDice)
        {
            targetDice->Render(context, gameContext.commonStates, viewerWorld, viewerView, viewerProj);
        }
    }

    if (m_isGameOver)
    {
        context->IASetInputLayout(colorInputLayout.Get());
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

        const wchar_t* resultStr = L"";
        if (m_isDraw)
        {
            resultStr = L"DRAW";
        }
        else if (m_winnerPlayer == myPlayerType)
        {
            resultStr = L"YOU WIN!";
        }
        else
        {
            resultStr = L"YOU LOSE...";
        }

        float textOffsetX = m_isDraw ? 150.0f : 130.0f;
        gameContext.debugRenderer.DrawText({dialogX + textOffsetX, dialogY + 30.0f}, DirectX::Colors::White, resultStr);
        gameContext.debugRenderer.DrawText({btnX + 65.0f, btnY1 + 12.0f}, DirectX::Colors::White, L"再戦");
        gameContext.debugRenderer.DrawText({btnX + 55.0f, btnY2 + 12.0f}, DirectX::Colors::White, L"タイトルへ");
    }
}