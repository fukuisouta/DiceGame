#include "pch.h"
#include "GamePlaySceneBase.h"
#include "Framework/SceneSerializer.h"
#include <algorithm>
#include <fstream>
#include <cmath>
#include <Mouse.h>
#include <Keyboard.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// コンストラクタ
GamePlaySceneBase::GamePlaySceneBase() = default;

// デストラクタ
GamePlaySceneBase::~GamePlaySceneBase()
{
    m_judgeManager.reset();

    m_selectionMarker.reset();
    m_colorEffect.reset();
    m_effect.reset();
    m_colorPrimitiveBatch.reset();
    m_primitiveBatch.reset();
}

// 画面中央通知の表示
void GamePlaySceneBase::TriggerTurnNotice(const std::wstring& text, float duration)
{
    m_turnManager.TriggerNotice(text, duration, &m_logManager);
}

// 移動状態のリセット
void GamePlaySceneBase::ResetMoveState()
{
    m_movementController.Reset();
}

// 注目ダイスインスタンスの取得
Dice* GamePlaySceneBase::GetTargetDiceInstance()
{
    int selX = m_movementController.GetSelectedX();
    int selY = m_movementController.GetSelectedY();
    if (selX >= 0 && selY >= 0)
    {
        const auto* piece = m_gameLogic.GetPieceAt(selX, selY);
        if (piece && piece->isActive && piece->diceInstance)
            return piece->diceInstance.get();
    }

    int curX = m_movementController.GetCursorX();
    int curY = m_movementController.GetCursorY();
    if (curX >= 0 && curY >= 0)
    {
        const auto* piece = m_gameLogic.GetPieceAt(curX, curY);
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

// 移動可能マスの算出
std::vector<MovableTileInfo> GamePlaySceneBase::GetMovableTiles() const
{
    return m_movementController.GetMovableTiles(m_gameLogic);
}

// シーン進入時の初期化
void GamePlaySceneBase::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Play);

    m_knockbackDiceList.clear();

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_field.Initialize(device, context, DiceBattleGame::BOARD_WIDTH, DiceBattleGame::BOARD_HEIGHT,
                       GamePlayBaseConstants::DEFAULT_GRID_CELL_SIZE);
    m_gameLogic.Initialize(device, context);

    m_gameLogic.SetOnPieceDefeatedCallback(
        [this, device, context](const Vector3& attackerPos, const Vector3& victimPos, const std::string& modelPath)
        {
            SpawnKnockbackDice(device, context, attackerPos, victimPos, modelPath);
            m_particleManager.SpawnClickParticles(victimPos.x, victimPos.z,
                                                  GamePlayBaseConstants::DEFEAT_PARTICLE_COUNT);
        });

    m_cameraManager.Initialize();
    m_uiController.Initialize(device, context);
    m_logManager.Initialize(device, context);
    m_particleManager.Initialize(device, context);

    LoadSceneFromJson(gameContext, "Resources/Data/scene_layout.json");

    m_judgeManager = std::make_unique<GameJudgeManager>();
    m_judgeManager->Initialize(device, context);

    m_turnManager.Initialize(m_gameLogic.GetCurrentTurn());
    m_movementController.Reset();

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    m_diceViewerController.Initialize(device, context, screenWidth / screenHeight);

    std::ifstream camFile("Resources/Data/camera_config.json");
    if (camFile.is_open())
    {
        try
        {
            nlohmann::json j;
            camFile >> j;
            if (j.contains("distance"))
                m_cameraManager.SetDistance(j["distance"].get<float>());
            if (j.contains("pitch"))
                m_cameraManager.SetPitch(j["pitch"].get<float>());
            if (j.contains("target"))
            {
                Vector3 target;
                target.x = j["target"]["x"].get<float>();
                target.y = j["target"]["y"].get<float>();
                target.z = j["target"]["z"].get<float>();
                m_cameraManager.SetTarget(target);
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

    m_ai.SetThinkDelay(GamePlayBaseConstants::DEFAULT_AI_THINK_DELAY);

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionTexture>>(context);
    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetTextureEnabled(true);
    m_effect->SetVertexColorEnabled(false);

    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    m_selectionMarker = DirectX::GeometricPrimitive::CreateCone(
        context, GamePlayBaseConstants::SELECTION_MARKER_SIZE, GamePlayBaseConstants::SELECTION_MARKER_HEIGHT,
        GamePlayBaseConstants::SELECTION_MARKER_TESSELLATION, false);
    m_markerAnimTimer = 0.0f;

    for (int i = 1; i <= GamePlayBaseConstants::DICE_FACE_COUNT; ++i)
    {
        std::wstring texPath = L"Resources/Textures/Count" + std::to_wstring(i) + L".png";
        DirectX::CreateWICTextureFromFile(device, texPath.c_str(), nullptr,
                                          m_diceCountTextures[i].ReleaseAndGetAddressOf());
    }

    m_particleManager.Clear();
    m_gameLogic.SetRemainingSteps(0);

    gameContext.networkManager.SetOnDiceResult(
        [this, &gameContext](int steps)
        {
            PlayerType myPlayerType = PlayerType::Player1;
            if (gameContext.networkManager.IsConnected() &&
                gameContext.networkManager.GetMode() == NetworkManager::Mode::Client)
            {
                myPlayerType = PlayerType::Player2;
            }

            if (m_gameLogic.GetCurrentTurn() == myPlayerType)
            {
                return;
            }

            m_gameLogic.SetDiceResultImmediately(steps);
            m_gameLogic.ClearJustLandedSteps();
            ResetMoveState();
            m_logManager.AddLogFormat(L"あいてがダイスを振り\n【%d】が出ました！", steps);
        });

    gameContext.networkManager.SetOnMovePiece(
        [this, &gameContext](int fX, int fY, int tX, int tY)
        {
            std::wstring noticeText;
            float noticeTimer = 0.0f;
            m_movementController.ExecuteMove(fX, fY, tX, tY, true, m_gameLogic, m_logManager, &gameContext, noticeText,
                                             noticeTimer);
            if (!noticeText.empty())
            {
                m_turnManager.TriggerNotice(noticeText, noticeTimer);
            }
        });

    OnModeEnter(gameContext);

    TriggerTurnNotice(L"GAME START", GamePlayBaseConstants::GAME_START_NOTICE_DURATION);
}

// JSONファイルからのステージオブジェクト読み込み
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

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float orthoHeight = 10.0f;
    float orthoWidth = orthoHeight * (screenWidth / screenHeight);

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

        std::string tagLower = objData.tag;
        std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
        if (tagLower == "camera")
        {
            m_uiController.SetCameraBtnFromPlacedObject(obj, orthoWidth, orthoHeight, screenWidth, screenHeight);
        }

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
                auto loadedModel = Model::CreateFromCMO(device, fullPath.c_str(), fx);
                if (loadedModel)
                {
                    obj.model = std::move(loadedModel);
                    m_loadedObjects.push_back(obj);
                }
            }
            catch (...)
            {
            }
        }
    }
}

// フレーム更新処理
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

    m_uiController.UpdateDebugAdjustment(kb, kbState);

    bool isClient = (gameContext.networkManager.IsConnected() &&
                     gameContext.networkManager.GetMode() == NetworkManager::Mode::Client);
    PlayerType myPlayerType = isClient ? PlayerType::Player2 : PlayerType::Player1;

    bool isGameOver = m_judgeManager ? m_judgeManager->IsGameOver() : false;
    bool isDiceViewerOpen = m_diceViewerController.IsOpen();

    RECT logRect{};
    m_uiController.Update(mouseState, screenWidth, screenHeight, isClient, m_gameLogic.GetCurrentTurn(), myPlayerType,
                          m_gameLogic.IsWaitingForRoll(), m_gameLogic.GetRemainingSteps(),
                          m_gameLogic.IsAnyPieceMoving(), isDiceViewerOpen, isGameOver, m_logManager.IsButtonHovered(),
                          (m_movementController.GetSelectedX() != -1), logRect);

    m_logManager.SetButtonRect(logRect);
    m_logManager.Update(mouseState, screenWidth, screenHeight);

    if (m_diceViewerController.Update(dt, mouseState, mouseTracker, kb, m_uiController, GetTargetDiceInstance(),
                                      screenWidth, screenHeight))
    {
        return;
    }

    bool isCtrlPressed = kbState.IsKeyDown(DirectX::Keyboard::Keys::LeftControl) ||
                         kbState.IsKeyDown(DirectX::Keyboard::Keys::RightControl);

    if (isCtrlPressed)
    {
        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::W))
        {
            if (m_judgeManager)
                m_judgeManager->SetResult(GameJudgeManager::ResultState::Win);
            return;
        }
        if (kb.IsKeyPressed(DirectX::Keyboard::Keys::M))
        {
            if (m_judgeManager)
                m_judgeManager->SetResult(GameJudgeManager::ResultState::Lose);
            return;
        }
    }

    m_cameraManager.Update(dt, kbState, kb, isClient);
    m_particleManager.Update(dt);

    m_turnManager.Update(dt, m_gameLogic, m_ai, m_logManager, gameContext.networkManager.IsConnected());

    if (m_judgeManager && m_judgeManager->IsGameOver())
    {
        GameJudgeManager::Action action = m_judgeManager->Update(mouseState, mouseTracker, kb);
        if (action == GameJudgeManager::Action::Retry)
        {
            OnEnter(gameContext);
        }
        else if (action == GameJudgeManager::Action::GoToTitle)
        {
            sceneController.RequestSwitch(SceneId::TitleScene);
        }
        return;
    }

    bool isPieceMoving = m_gameLogic.IsAnyPieceMoving();
    bool isKnockbackActive = !m_knockbackDiceList.empty();

    if (m_judgeManager && !isPieceMoving && !isKnockbackActive)
    {
        m_judgeManager->JudgeGame(m_gameLogic, myPlayerType);
        if (m_judgeManager->IsGameOver())
        {
            auto state = m_judgeManager->GetResultState();
            if (state == GameJudgeManager::ResultState::Draw)
                AddLog(L"GAME END : DRAW");
            else if (state == GameJudgeManager::ResultState::Win)
                AddLog(L"GAME END : YOU WIN");
            else if (state == GameJudgeManager::ResultState::Lose)
                AddLog(L"GAME END : YOU LOSE");
            return;
        }
    }

    if (!m_dropAnimator.IsFinished())
    {
        m_dropAnimator.Update(dt);
        return;
    }

    OnModeUpdate(dt, gameContext);

    m_gameLogic.Update(dt);
    m_field.Update(dt);

    for (auto it = m_knockbackDiceList.begin(); it != m_knockbackDiceList.end();)
    {
        (*it)->Update(dt);
        if (!(*it)->IsActive())
        {
            it = m_knockbackDiceList.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (m_gameLogic.GetJustLandedSteps() > 0)
    {
        int steps = m_gameLogic.GetJustLandedSteps();

        if (steps > 0)
        {
            PlayerType currentTurn = m_gameLogic.GetCurrentTurn();

            if (currentTurn == myPlayerType)
            {
                m_logManager.AddLogFormat(L"あなたがダイスを振り\n【%d】が出ました！", steps);

                if (gameContext.networkManager.IsConnected())
                {
                    gameContext.networkManager.SendDiceResult(steps);
                }
            }
        }
        m_gameLogic.ClearJustLandedSteps();
    }

    if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !isDiceViewerOpen)
    {
        if (m_logManager.IsButtonClicked(mouseTracker))
        {
            m_logManager.ToggleWindow();
            m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                                  GamePlayBaseConstants::LOG_BUTTON_CLICK_PARTICLE_COUNT);
        }
        else if (m_uiController.IsCameraBtnHovered())
        {
            m_cameraManager.ToggleView();
            m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                                  GamePlayBaseConstants::CAMERA_BUTTON_CLICK_PARTICLE_COUNT);
        }
    }

    bool canRoll = (!m_gameLogic.IsWaitingForRoll() && !m_gameLogic.IsGameOver());
    float aspectRatio = screenWidth / screenHeight;

    Viewport viewport(0.0f, 0.0f, screenWidth, screenHeight);
    Matrix view =
        Matrix::CreateLookAt(m_cameraManager.GetEye(), m_cameraManager.GetTarget(), Vector3(0.0f, 1.0f, 0.0f));
    Matrix proj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(GamePlayBaseConstants::FOV_ANGLE_Y), aspectRatio,
                                             GamePlayBaseConstants::NEAR_PLANE, GamePlayBaseConstants::FAR_PLANE);

    if (m_gameLogic.GetCurrentTurn() == myPlayerType)
    {
        m_gameLogic.SetPlayerShield(myPlayerType, false);

        if (mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED && !m_logManager.IsButtonHovered() &&
            !m_uiController.IsDiceCheckBtnHovered() && !m_uiController.IsCameraBtnHovered())
        {
            m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x), static_cast<float>(mouseState.y),
                                                  GamePlayBaseConstants::MOUSE_CLICK_PARTICLE_COUNT);
        }

        if (canRoll && (m_gameLogic.GetRemainingSteps() == 0) && m_uiController.IsRollButtonHovered() &&
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
                !m_logManager.IsButtonHovered() && !m_uiController.IsDiceCheckBtnHovered() &&
                !m_uiController.IsCameraBtnHovered())
            {
                OnModeGridClicked(hoveredGridX, hoveredGridY);
            }
        }
        else
        {
            m_movementController.Update(mouseState, mouseTracker, kb, kbState, m_gameLogic, m_logManager,
                                        m_uiController, hoveredGridX, hoveredGridY, isClient, myPlayerType,
                                        isCtrlPressed, &gameContext);
        }
    }

    m_gameLogic.SetCursorPosition(m_movementController.GetCursorX(), m_movementController.GetCursorY());
}

// 描画処理
void GamePlaySceneBase::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);
    float aspectRatio = screenWidth / screenHeight;

    Matrix view =
        Matrix::CreateLookAt(m_cameraManager.GetEye(), m_cameraManager.GetTarget(), Vector3(0.0f, 1.0f, 0.0f));
    Matrix proj =
        Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(GamePlayBaseConstants::FOV_ANGLE_Y), aspectRatio,
                                             GamePlayBaseConstants::NEAR_PLANE, GamePlayBaseConstants::FAR_PLANE);

    std::vector<MovableTileInfo> movableTiles = GetMovableTiles();

    ID3D11ShaderResourceView* rawFaceTextures[7] = {nullptr};
    for (int i = 1; i <= GamePlayBaseConstants::DICE_FACE_COUNT; ++i)
    {
        rawFaceTextures[i] = m_diceCountTextures[i].Get();
    }

    int curX = m_movementController.GetCursorX();
    int curY = m_movementController.GetCursorY();
    int selX = m_movementController.GetSelectedX();
    int selY = m_movementController.GetSelectedY();

    m_field.Render(context, gameContext.commonStates, view, proj, curX, curY, selX, selY, false, movableTiles,
                   rawFaceTextures);
    m_gameLogic.Render(context, gameContext.commonStates, view, proj);

    for (const auto& kbDice : m_knockbackDiceList)
    {
        kbDice->Render(context, gameContext.commonStates, view, proj);
    }

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

    OnModeRender2D(context, screenWidth, screenHeight, gameContext);

    void const* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    device->CreateInputLayout(VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount,
                              shaderByteCode, byteCodeLength, inputLayout.GetAddressOf());

    bool isGameOver = m_judgeManager ? m_judgeManager->IsGameOver() : false;
    bool isDiceViewerOpen = m_diceViewerController.IsOpen();

    if (!isDiceViewerOpen && !isGameOver)
    {
        m_logManager.RenderButton(context, m_primitiveBatch.get(), m_effect.get(), inputLayout.Get());
    }

    bool isDraw = (m_judgeManager && m_judgeManager->GetResultState() == GameJudgeManager::ResultState::Draw);
    PlayerType winnerPlayer = (m_judgeManager && m_judgeManager->GetResultState() == GameJudgeManager::ResultState::Win)
                                  ? myPlayerType
                                  : (myPlayerType == PlayerType::Player1 ? PlayerType::Player2 : PlayerType::Player1);

    m_uiController.Render(context, gameContext.commonStates, device, screenWidth, screenHeight,
                          m_gameLogic.GetCurrentTurn(), myPlayerType, m_gameLogic.IsWaitingForRoll(),
                          m_gameLogic.GetRemainingSteps(), isDiceViewerOpen, isGameOver, isDraw, winnerPlayer,
                          m_turnManager.GetNoticeText(), m_turnManager.GetNoticeTimer(), m_logManager.IsButtonHovered(),
                          m_logManager.GetButtonRect());

    void const* colorByteCode = nullptr;
    size_t colorByteCodeLength = 0;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, colorByteCode,
                              colorByteCodeLength, colorInputLayout.GetAddressOf());

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
    context->RSSetState(gameContext.commonStates.CullNone());

    Matrix particleOrthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(particleOrthoProj);

    context->IASetInputLayout(colorInputLayout.Get());
    m_particleManager.Render(context, m_colorPrimitiveBatch.get(), m_colorEffect.get());

    m_logManager.Render(context, gameContext.commonStates, gameContext.debugRenderer, m_colorPrimitiveBatch.get(),
                        m_colorEffect.get(), screenWidth, screenHeight);

    if (m_turnManager.IsNoticeActive() && !isDiceViewerOpen)
    {
        const auto& noticeText = m_turnManager.GetNoticeText();
        float textX = (screenWidth * GamePlayBaseConstants::NOTICE_TEXT_X_RATIO) -
                      (static_cast<float>(noticeText.length()) * GamePlayBaseConstants::NOTICE_TEXT_CHAR_WIDTH);
        float textY = screenHeight * GamePlayBaseConstants::NOTICE_TEXT_Y_RATIO;

        context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

        Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
        m_colorEffect->SetWorld(Matrix::Identity);
        m_colorEffect->SetView(Matrix::Identity);
        m_colorEffect->SetProjection(orthoProj);

        context->IASetInputLayout(colorInputLayout.Get());
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

        gameContext.debugRenderer.DrawText({textX, textY}, DirectX::Colors::White, noticeText.c_str());

        m_colorEffect->SetWorld(Matrix::Identity);
        m_colorEffect->SetView(Matrix::Identity);
        m_colorEffect->SetProjection(Matrix::Identity);
        context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    }

    m_diceViewerController.Render(context, gameContext.commonStates, gameContext.debugRenderer, m_uiController,
                                  GetTargetDiceInstance(), screenWidth, screenHeight);

    if (m_judgeManager)
    {
        m_judgeManager->Render(context, gameContext.commonStates, gameContext.debugRenderer, screenWidth, screenHeight);
    }
}