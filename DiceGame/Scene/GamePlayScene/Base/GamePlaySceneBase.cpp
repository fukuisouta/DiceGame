#include "pch.h"
#include "GamePlaySceneBase.h"
#include "Framework/SceneSerializer.h"
#include <algorithm>
#include <fstream>
#include <cmath>
#include <random>
#include <Mouse.h>
#include <Keyboard.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

GamePlaySceneBase::GamePlaySceneBase() = default;

GamePlaySceneBase::~GamePlaySceneBase()
{
    m_judgeManager.reset();
    m_selectionMarker.reset();
    m_colorEffect.reset();
    m_effect.reset();
    m_colorPrimitiveBatch.reset();
    m_primitiveBatch.reset();
}

void GamePlaySceneBase::TriggerTurnNotice(const std::wstring& text, float duration)
{
    m_turnManager.TriggerNotice(text, duration, &m_logManager);
}

void GamePlaySceneBase::ResetMoveState()
{
    m_movementController.Reset();
}

// 出目(1〜6)が上面(+Y)に来るクォータニオンを返す（StepDice.h / Dice.cpp の面定義と完全一致）
Quaternion GamePlaySceneBase::GetDiceFaceRotation(int face)
{
    switch (face)
    {
        case 1:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitZ, XM_PIDIV2);
        case 2:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitX, -XM_PIDIV2);
        case 3:
            return Quaternion::Identity;
        case 4:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitX, XM_PI);
        case 5:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitX, XM_PIDIV2);
        case 6:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -XM_PIDIV2);
        default:
            return Quaternion::Identity;
    }
}

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

std::vector<MovableTileInfo> GamePlaySceneBase::GetMovableTiles() const
{
    return m_movementController.GetMovableTiles(m_gameLogic);
}

void GamePlaySceneBase::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Play);

    m_knockbackDiceList.clear();
    m_hasClickedRollThisTurn = false;

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_field.Initialize(device, context, DiceBattleGame::BOARD_WIDTH, DiceBattleGame::BOARD_HEIGHT,
                       GamePlayBaseConstants::DEFAULT_GRID_CELL_SIZE);
    m_gameLogic.Initialize(device, context);

    // 先攻決定用ダイス（1P＝青ダイス、2P＝赤ダイス）を盤面中央に配置
    m_p1FirstDice.Initialize(device, context, "Resources/Models/BlueDice.cmo");
    m_p1FirstDice.SetPosition(Vector3(-2.2f, 2.5f, 0.0f));
    m_p1FirstDice.SetRotation(Quaternion::Identity);

    m_p2FirstDice.Initialize(device, context, "Resources/Models/RedDice.cmo");
    m_p2FirstDice.SetPosition(Vector3(2.2f, 2.5f, 0.0f));
    m_p2FirstDice.SetRotation(Quaternion::Identity);

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

    // サーバーからの先攻決定結果を受信するコールバック
    gameContext.networkManager.SetOnFirstTurnResult(
        [this](int p1Roll, int p2Roll, uint8_t firstPlayer)
        {
            m_p1ResultRoll = p1Roll;
            m_p2ResultRoll = p2Roll;
            m_decidedFirstPlayer = firstPlayer;
            m_hasReceivedFirstResult = true;
        });

    // 相手がダイスを振った際の結果受信コールバック
    gameContext.networkManager.SetOnDiceResult(
        [this, &gameContext](int steps)
        {
            bool isHost = (gameContext.networkManager.GetAssignedPlayerType() == 1);
            PlayerType myPlayerType = isHost ? PlayerType::Player1 : PlayerType::Player2;

            if (m_gameLogic.GetCurrentTurn() == myPlayerType)
            {
                return;
            }

            if (m_gameLogic.IsAnyPieceMoving())
            {
                m_gameLogic.FinishAllPieceAnimations();
            }
            m_gameLogic.StartTurnWithSteps(steps);
            m_gameLogic.ClearJustLandedSteps();
            ResetMoveState();
            m_logManager.AddLogFormat(L"あいてがダイスを振り\n【%d】が出ました！", steps);
        });

    // 相手がコマを動かした際の受信コールバック
    gameContext.networkManager.SetOnMovePiece(
        [this, &gameContext](int fX, int fY, int tX, int tY)
        {
            bool isHost = (gameContext.networkManager.GetAssignedPlayerType() == 1);

            if (m_gameLogic.IsWaitingForRoll())
            {
                int pendingSteps = m_gameLogic.GetTargetFixedSteps();
                m_gameLogic.SetDiceResultImmediately(pendingSteps > 0 ? pendingSteps : 1);
            }

            // 連打で前のアニメが終わっていない場合のみスナップ（通常の1歩移動は滑らかに補間させる）
            if (m_gameLogic.IsAnyPieceMoving())
            {
                m_gameLogic.FinishAllPieceAnimations();
            }

            int prevTurnCount = m_gameLogic.GetCurrentTurnCount();

            std::wstring noticeText;
            float noticeTimer = 0.0f;
            m_movementController.ExecuteMove(fX, fY, tX, tY, true, m_gameLogic, m_logManager, &gameContext, noticeText,
                                             noticeTimer);
            if (!noticeText.empty())
            {
                m_turnManager.TriggerNotice(noticeText, noticeTimer);
            }

            int nowTurnCount = m_gameLogic.GetCurrentTurnCount();
            if (nowTurnCount > prevTurnCount && !m_gameLogic.IsGameOver())
            {
                AddLog(L"【%d / %d手完了】陣地 青:%dマス vs 赤:%dマス", nowTurnCount, DiceBattleConstants::MAX_TURNS,
                       m_gameLogic.GetPaintedTileCount(PlayerType::Player1),
                       m_gameLogic.GetPaintedTileCount(PlayerType::Player2));
            }

            // ホスト(1P)は2Pの移動を確定させた直後に正本盤面（出目・位置）を2Pへ配信
            if (isHost)
            {
                SendHostWorldSync(gameContext);
            }
        });

    // 相手からターン終了パケットを受信したら確実にターンを切り替える
    gameContext.networkManager.SetOnEndTurn(
        [this, &gameContext]()
        {
            bool isHost = (gameContext.networkManager.GetAssignedPlayerType() == 1);
            PlayerType myPlayerType = isHost ? PlayerType::Player1 : PlayerType::Player2;

            if (m_gameLogic.IsAnyPieceMoving())
            {
                m_gameLogic.FinishAllPieceAnimations();
            }
            m_hasClickedRollThisTurn = false;

            if (m_gameLogic.GetCurrentTurn() != myPlayerType)
            {
                int prevTurnCount = m_gameLogic.GetCurrentTurnCount();
                m_gameLogic.ForceEndTurn();
                ResetMoveState();

                int nowTurnCount = m_gameLogic.GetCurrentTurnCount();
                if (nowTurnCount > prevTurnCount && !m_gameLogic.IsGameOver())
                {
                    AddLog(L"【%d / %d手完了】陣地 青:%dマス vs 赤:%dマス", nowTurnCount,
                           DiceBattleConstants::MAX_TURNS, m_gameLogic.GetPaintedTileCount(PlayerType::Player1),
                           m_gameLogic.GetPaintedTileCount(PlayerType::Player2));
                }

                if (!m_gameLogic.IsGameOver())
                {
                    TriggerTurnNotice(L"YOUR TURN");
                }
            }

            // ホスト(1P)はターン切り替え確定後に盤面・出目を2Pへ配信
            if (isHost)
            {
                SendHostWorldSync(gameContext);
            }
        });

    // ゲスト(2P)専用：ホスト(1P)からの正本盤面（出目・位置・歩数ダイス姿勢）をそのまま正として適用
    gameContext.networkManager.SetOnWorldSync(
        [this, &gameContext](const MsgWorldSync& syncPkt)
        {
            bool isHost = (gameContext.networkManager.GetAssignedPlayerType() == 1);
            if (isHost)
            {
                return; // ホスト自身が正本なので上書きしない
            }

            // アニメーション中でなければホストの確定状態をそのまま反映
            if (!m_gameLogic.IsAnyPieceMoving())
            {
                m_gameLogic.ApplyWorldSyncFromHost(syncPkt);
            }
        });

    OnModeEnter(gameContext);

    // シーン進入直後に先攻決定ダイスロールを開始
    StartFirstTurnRoll(gameContext);
}

// ホスト(1P)の正本状態をゲスト(2P)へ送信するヘルパー
void GamePlaySceneBase::SendHostWorldSync(GameContext& gameContext)
{
    if (!gameContext.networkManager.IsConnected())
        return;
    if (gameContext.networkManager.GetAssignedPlayerType() != 1)
        return;

    MsgWorldSync syncPkt{};
    m_gameLogic.BuildWorldSyncPacket(syncPkt);
    gameContext.networkManager.SendPacket(&syncPkt, sizeof(syncPkt));
}

void GamePlaySceneBase::StartFirstTurnRoll(GameContext& gameContext)
{
    m_firstTurnPhase = FirstTurnPhase::Rolling;
    m_firstRollTotalDuration = 2.0f;
    m_firstRollTimer = m_firstRollTotalDuration;
    m_hasReceivedFirstResult = false;
    m_decidedFirstPlayer = 0;

    // 発光リセット
    m_p1FirstDice.SetEmissiveColor(Vector3::Zero);
    m_p2FirstDice.SetEmissiveColor(Vector3::Zero);

    // 盤面中央の着地位置（駒に埋まらない高さ Y=2.2f）
    m_p1FirstTargetPos = Vector3(-2.2f, 2.2f, 0.0f);
    m_p2FirstTargetPos = Vector3(2.2f, 2.2f, 0.0f);

    // 画面手前（左右斜め手前）から中央に向かって投げ込む開始位置
    m_p1FirstStartPos = Vector3(-5.5f, 3.5f, -9.5f);
    m_p2FirstStartPos = Vector3(5.5f, 3.5f, 9.5f);

    m_p1FirstDice.SetPosition(m_p1FirstStartPos);
    m_p2FirstDice.SetPosition(m_p2FirstStartPos);

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 6);
    m_myFirstRollValue = dist(rng);

    SoundManager::GetInstance().PlaySE(SEType::DiceRoll);

    if (gameContext.networkManager.IsConnected())
    {
        gameContext.networkManager.SendFirstTurnRoll(m_myFirstRollValue);
    }
    else
    {
        m_p1ResultRoll = m_myFirstRollValue;
        m_p2ResultRoll = dist(rng);
        while (m_p1ResultRoll == m_p2ResultRoll)
        {
            m_p2ResultRoll = dist(rng);
        }
        m_decidedFirstPlayer = (m_p1ResultRoll > m_p2ResultRoll) ? 1 : 2;
        m_hasReceivedFirstResult = true;
    }
}

void GamePlaySceneBase::UpdateFirstTurnPhase(float dt, GameContext& gameContext)
{
    bool isPlayer2 =
        (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetAssignedPlayerType() == 2);

    // 暗転＆スポットライトのフェード制御（ゆっくり暗転・ゆっくりフェードアウト）
    if (m_firstTurnPhase == FirstTurnPhase::Rolling || m_firstTurnPhase == FirstTurnPhase::WaitingServer)
    {
        m_spotlightAlpha = (std::min)(1.0f, m_spotlightAlpha + dt * 2.0f);
    }
    else if (m_firstTurnPhase == FirstTurnPhase::ShowingResult && m_firstRollTimer < 0.6f && m_decidedFirstPlayer != 0)
    {
        m_spotlightAlpha = (std::max)(0.0f, m_firstRollTimer / 0.6f);
    }

    if (m_firstTurnPhase == FirstTurnPhase::Rolling)
    {
        m_firstRollTimer -= dt;

        if (m_firstRollTimer > 0.0f)
        {
            float t = 1.0f - (m_firstRollTimer / m_firstRollTotalDuration);

            // 前半60%でゆったり放物線飛行、後半40%で2回バウンドしながら減速着地
            auto CalcThrowPos = [](const Vector3& start, const Vector3& target, float progress) -> Vector3
            {
                if (progress < 0.60f)
                {
                    float airT = progress / 0.60f;
                    Vector3 pos = Vector3::Lerp(start, target, airT);
                    pos.y += std::sin(airT * XM_PI) * 4.5f;
                    return pos;
                }
                else
                {
                    float bounceT = (progress - 0.60f) / 0.40f;
                    Vector3 pos = target;
                    pos.y += std::abs(std::sin(bounceT * XM_PI * 2.0f)) * (1.0f - bounceT) * 1.2f;
                    return pos;
                }
            };

            m_p1FirstDice.SetPosition(CalcThrowPos(m_p1FirstStartPos, m_p1FirstTargetPos, t));
            m_p2FirstDice.SetPosition(CalcThrowPos(m_p2FirstStartPos, m_p2FirstTargetPos, t));

            // 回転スピードも少し落ち着かせて出目が見えやすくする
            Quaternion spin1 = Quaternion::CreateFromYawPitchRoll(dt * 11.0f, dt * 9.0f, dt * 7.5f);
            Quaternion spin2 = Quaternion::CreateFromYawPitchRoll(-dt * 10.5f, dt * 9.5f, -dt * 8.0f);

            if (t < 0.60f)
            {
                m_p1FirstDice.SetRotation(m_p1FirstDice.GetRotation() * spin1);
                m_p2FirstDice.SetRotation(m_p2FirstDice.GetRotation() * spin2);
            }
            else
            {
                float snapT = ((t - 0.60f) / 0.40f) * 0.35f;
                Quaternion myTargetRot = GetDiceFaceRotation(m_myFirstRollValue);

                if (isPlayer2)
                {
                    m_p1FirstDice.SetRotation(m_p1FirstDice.GetRotation() * spin1);
                    m_p2FirstDice.SetRotation(Quaternion::Slerp(m_p2FirstDice.GetRotation(), myTargetRot, snapT));
                }
                else
                {
                    m_p1FirstDice.SetRotation(Quaternion::Slerp(m_p1FirstDice.GetRotation(), myTargetRot, snapT));
                    m_p2FirstDice.SetRotation(m_p2FirstDice.GetRotation() * spin2);
                }
            }
        }
        else
        {
            m_p1FirstDice.SetPosition(m_p1FirstTargetPos);
            m_p2FirstDice.SetPosition(m_p2FirstTargetPos);

            if (isPlayer2)
                m_p2FirstDice.SetRotation(GetDiceFaceRotation(m_myFirstRollValue));
            else
                m_p1FirstDice.SetRotation(GetDiceFaceRotation(m_myFirstRollValue));

            SoundManager::GetInstance().PlaySE(SEType::DiceRoll, DiceBattleConstants::DICE_SE_VOLUME_LAND);

            // 着地後、すぐ結果を出さずに0.6秒間の「タメ（緊張の間）」を作る
            m_firstRollTimer = 0.6f;
            m_firstTurnPhase = FirstTurnPhase::WaitingServer;
        }
    }
    else if (m_firstTurnPhase == FirstTurnPhase::WaitingServer)
    {
        if (m_firstRollTimer > 0.0f)
        {
            m_firstRollTimer -= dt;
        }

        // 通信結果未着、またはタメ時間経過前は待機
        if (!m_hasReceivedFirstResult || m_firstRollTimer > 0.0f)
        {
            if (!m_hasReceivedFirstResult)
            {
                Quaternion waitSpin = Quaternion::CreateFromYawPitchRoll(dt * 11.0f, dt * 9.0f, dt * 7.5f);
                if (isPlayer2)
                    m_p1FirstDice.SetRotation(m_p1FirstDice.GetRotation() * waitSpin);
                else
                    m_p2FirstDice.SetRotation(m_p2FirstDice.GetRotation() * waitSpin);
            }
            else
            {
                // オフライン時は相手のダイスも確定出目でピタッと止めてタメを作る
                m_p1FirstDice.SetRotation(GetDiceFaceRotation(m_p1ResultRoll));
                m_p2FirstDice.SetRotation(GetDiceFaceRotation(m_p2ResultRoll));
            }
            return;
        }

        // タメ終了 → スポットライト点灯＆勝敗発表！
        m_p1FirstDice.SetRotation(GetDiceFaceRotation(m_p1ResultRoll));
        m_p2FirstDice.SetRotation(GetDiceFaceRotation(m_p2ResultRoll));
        m_firstRollTimer = 2.8f;
        m_firstTurnPhase = FirstTurnPhase::ShowingResult;

        if (m_decidedFirstPlayer == 1)
            m_p1FirstDice.SetEmissiveColor(Vector3(0.45f, 0.4f, 0.1f));
        else if (m_decidedFirstPlayer == 2)
            m_p2FirstDice.SetEmissiveColor(Vector3(0.45f, 0.4f, 0.1f));

        if (m_decidedFirstPlayer == 0)
        {
            AddLog(L"先攻決めダイス：【%d vs %d】引き分け！再ロールします", m_p1ResultRoll, m_p2ResultRoll);
        }
        else
        {
            uint8_t myRole =
                gameContext.networkManager.IsConnected() ? gameContext.networkManager.GetAssignedPlayerType() : 1;
            bool amIFirst = (m_decidedFirstPlayer == myRole);
            AddLog(L"先攻決め：青(%d) vs 赤(%d) -> %s", m_p1ResultRoll, m_p2ResultRoll,
                   amIFirst ? L"あなたが先攻！" : L"あいてが先攻！");
        }
    }
    else if (m_firstTurnPhase == FirstTurnPhase::ShowingResult)
    {
        m_firstRollTimer -= dt;
        if (m_firstRollTimer <= 0.0f)
        {
            if (m_decidedFirstPlayer == 0)
            {
                StartFirstTurnRoll(gameContext);
            }
            else
            {
                m_spotlightAlpha = 0.0f;
                PlayerType firstPlayer = (m_decidedFirstPlayer == 2) ? PlayerType::Player2 : PlayerType::Player1;
                m_gameLogic.SetFirstTurnPlayer(firstPlayer);
                m_turnManager.Initialize(firstPlayer);
                m_hasClickedRollThisTurn = false;
                m_firstTurnPhase = FirstTurnPhase::Done;

                SendHostWorldSync(gameContext);
                TriggerTurnNotice(L"GAME START", GamePlayBaseConstants::GAME_START_NOTICE_DURATION);
            }
        }
    }
}

void GamePlaySceneBase::RenderFirstTurnSpotlight(ID3D11DeviceContext* context, const DirectX::CommonStates& states,
                                                 const DirectX::SimpleMath::Matrix& view,
                                                 const DirectX::SimpleMath::Matrix& proj)
{
    if (m_spotlightAlpha <= 0.001f)
        return;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (!device)
        return;

    void const* colorByteCode = nullptr;
    size_t colorByteCodeLength = 0;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, colorByteCode,
                              colorByteCodeLength, colorInputLayout.GetAddressOf());

    context->IASetInputLayout(colorInputLayout.Get());
    context->RSSetState(states.CullNone());
    context->OMSetDepthStencilState(states.DepthNone(), 0);
    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);

    // 1. 画面全体（背景・台座・盤面・初期駒すべて）を2Dで一括暗転
    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(Matrix::CreateOrthographicOffCenter(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f));
    m_colorEffect->Apply(context);

    m_colorPrimitiveBatch->Begin();
    {
        Vector4 dimCol(0.0f, 0.0f, 0.0f, 0.68f * m_spotlightAlpha);
        VertexPositionColor v0(Vector3(-1.0f, -1.0f, 0.0f), dimCol);
        VertexPositionColor v1(Vector3(-1.0f, 1.0f, 0.0f), dimCol);
        VertexPositionColor v2(Vector3(1.0f, 1.0f, 0.0f), dimCol);
        VertexPositionColor v3(Vector3(1.0f, -1.0f, 0.0f), dimCol);
        m_colorPrimitiveBatch->DrawQuad(v0, v1, v2, v3);
    }
    m_colorPrimitiveBatch->End();

    // 2. 勝敗確定後、勝った（先攻になった）方だけに3Dピンスポットライトを照射
    if (m_firstTurnPhase == FirstTurnPhase::ShowingResult && m_decidedFirstPlayer != 0)
    {
        m_colorEffect->SetWorld(Matrix::Identity);
        m_colorEffect->SetView(view);
        m_colorEffect->SetProjection(proj);
        context->OMSetBlendState(states.Additive(), nullptr, 0xFFFFFFFF);
        m_colorEffect->Apply(context);

        auto DrawSingleSpot = [&](const Vector3& centerPos, const Vector4& tintColor)
        {
            const int segments = 32;
            float topY = 14.0f;
            float botY = 1.2f;
            float topRadius = 0.45f;
            float botRadius = 2.6f;

            Vector3 topCenter(centerPos.x, topY, centerPos.z);
            Vector3 botCenter(centerPos.x, botY, centerPos.z);

            Vector4 topCol(tintColor.x, tintColor.y, tintColor.z, 0.45f * m_spotlightAlpha);
            Vector4 edgeCol(tintColor.x, tintColor.y, tintColor.z, 0.0f);
            Vector4 floorCenterCol(tintColor.x, tintColor.y, tintColor.z, 0.65f * m_spotlightAlpha);

            for (int i = 0; i < segments; ++i)
            {
                float a0 = (XM_2PI * i) / segments;
                float a1 = (XM_2PI * (i + 1)) / segments;

                Vector3 t0(topCenter.x + std::cos(a0) * topRadius, topY, topCenter.z + std::sin(a0) * topRadius);
                Vector3 t1(topCenter.x + std::cos(a1) * topRadius, topY, topCenter.z + std::sin(a1) * topRadius);
                Vector3 b0(botCenter.x + std::cos(a0) * botRadius, botY, botCenter.z + std::sin(a0) * botRadius);
                Vector3 b1(botCenter.x + std::cos(a1) * botRadius, botY, botCenter.z + std::sin(a1) * botRadius);

                m_colorPrimitiveBatch->DrawQuad(VertexPositionColor(t0, topCol), VertexPositionColor(t1, topCol),
                                                VertexPositionColor(b1, edgeCol), VertexPositionColor(b0, edgeCol));
                m_colorPrimitiveBatch->DrawTriangle(VertexPositionColor(botCenter, floorCenterCol),
                                                    VertexPositionColor(b0, edgeCol), VertexPositionColor(b1, edgeCol));
            }
        };

        m_colorPrimitiveBatch->Begin();
        if (m_decidedFirstPlayer == 1)
        {
            DrawSingleSpot(m_p1FirstTargetPos, Vector4(0.85f, 0.93f, 1.0f, 1.0f));
        }
        else if (m_decidedFirstPlayer == 2)
        {
            DrawSingleSpot(m_p2FirstTargetPos, Vector4(1.0f, 0.88f, 0.80f, 1.0f));
        }
        m_colorPrimitiveBatch->End();
    }

    context->OMSetBlendState(states.Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.DepthDefault(), 0);
    context->RSSetState(states.CullCounterClockwise());
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

    bool isPlayer2 =
        (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetAssignedPlayerType() == 2);
    PlayerType myPlayerType = isPlayer2 ? PlayerType::Player2 : PlayerType::Player1;

    m_cameraManager.Update(dt, kbState, kb, isPlayer2);
    m_particleManager.Update(dt);

    // 1. コマ落下アニメーション中
    if (!m_dropAnimator.IsFinished())
    {
        m_dropAnimator.Update(dt);
        return;
    }

    // 2. 先攻・後攻決定ダイスロールフェーズ中
    if (m_firstTurnPhase != FirstTurnPhase::Done)
    {
        UpdateFirstTurnPhase(dt, gameContext);
        return;
    }

    bool isGameOver = m_judgeManager ? m_judgeManager->IsGameOver() : false;
    bool isDiceViewerOpen = m_diceViewerController.IsOpen();

    RECT logRect{};
    m_uiController.Update(mouseState, screenWidth, screenHeight, isPlayer2, m_gameLogic.GetCurrentTurn(), myPlayerType,
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

    int prevTurnCount = m_gameLogic.GetCurrentTurnCount();
    m_turnManager.Update(dt, m_gameLogic, m_ai, m_logManager, gameContext.networkManager.IsConnected(), myPlayerType);
    int nowTurnCount = m_gameLogic.GetCurrentTurnCount();
    if (nowTurnCount > prevTurnCount && !m_gameLogic.IsGameOver())
    {
        AddLog(L"【%d / %d手完了】陣地 青:%dマス vs 赤:%dマス", nowTurnCount, DiceBattleConstants::MAX_TURNS,
               m_gameLogic.GetPaintedTileCount(PlayerType::Player1),
               m_gameLogic.GetPaintedTileCount(PlayerType::Player2));
    }

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
            int p1Tiles = m_gameLogic.GetPaintedTileCount(PlayerType::Player1);
            int p2Tiles = m_gameLogic.GetPaintedTileCount(PlayerType::Player2);

            if (m_gameLogic.GetCurrentTurnCount() >= DiceBattleConstants::MAX_TURNS)
            {
                AddLog(L"20手経過！陣地判定【青:%dマス vs 赤:%dマス】", p1Tiles, p2Tiles);
            }

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
                m_logManager.AddLogFormat(L"あなたがダイスを振り【%d】が出ました！", steps);

                if (gameContext.networkManager.IsConnected())
                {
                    gameContext.networkManager.SendDiceResult(steps);
                    // ホスト(1P)が振った場合のみ正本状態を2Pへ送信
                    SendHostWorldSync(gameContext);
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

    // 相手ターンになったらロールロックを解除して次の自分のターンに備える
    if (m_gameLogic.GetCurrentTurn() != myPlayerType)
    {
        m_hasClickedRollThisTurn = false;
    }

    bool canRoll = (!m_gameLogic.IsWaitingForRoll() && !m_gameLogic.IsGameOver() && !m_hasClickedRollThisTurn &&
                    !m_movementController.HasMovedThisTurn());
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
            m_hasClickedRollThisTurn = true;
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
        else if (!m_gameLogic.IsAnyPieceMoving() && !m_gameLogic.IsWaitingForRoll())
        {
            int prevMoveTurnCount = m_gameLogic.GetCurrentTurnCount();
            m_movementController.Update(mouseState, mouseTracker, kb, kbState, m_gameLogic, m_logManager,
                                        m_uiController, hoveredGridX, hoveredGridY, isPlayer2, myPlayerType,
                                        isCtrlPressed, &gameContext);
            int nowMoveTurnCount = m_gameLogic.GetCurrentTurnCount();
            if (nowMoveTurnCount > prevMoveTurnCount && !m_gameLogic.IsGameOver())
            {
                AddLog(L"【%d / %d手完了】陣地 青:%dマス vs 赤:%dマス", nowMoveTurnCount,
                       DiceBattleConstants::MAX_TURNS, m_gameLogic.GetPaintedTileCount(PlayerType::Player1),
                       m_gameLogic.GetPaintedTileCount(PlayerType::Player2));
            }
        }
    }

    m_gameLogic.SetCursorPosition(m_movementController.GetCursorX(), m_movementController.GetCursorY());
}

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

    // 盤面（陣地カラー配列を含む）を描画
    m_field.Render(context, gameContext.commonStates, view, proj, curX, curY, selX, selY, false, movableTiles,
                   rawFaceTextures, -1, -1, m_gameLogic.GetTileColors());
    m_gameLogic.Render(context, gameContext.commonStates, view, proj);

    // 先攻決定フェーズ中は暗転＋スポットライト＋青赤ダイスを最前面に描画
    if (m_firstTurnPhase != FirstTurnPhase::Done && m_dropAnimator.IsFinished())
    {
        RenderFirstTurnSpotlight(context, gameContext.commonStates, view, proj);
        m_p1FirstDice.Render(context, gameContext.commonStates, view, proj);
        m_p2FirstDice.Render(context, gameContext.commonStates, view, proj);
    }

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

    bool isPlayer2 =
        (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetAssignedPlayerType() == 2);
    PlayerType myPlayerType = isPlayer2 ? PlayerType::Player2 : PlayerType::Player1;

    // 先攻決定フェーズ中の2D案内テキスト描画
    if (m_firstTurnPhase != FirstTurnPhase::Done && m_dropAnimator.IsFinished())
    {
        wchar_t msgBuf[128] = {};
        if (m_firstTurnPhase == FirstTurnPhase::Rolling || m_firstTurnPhase == FirstTurnPhase::WaitingServer)
        {
            swprintf_s(msgBuf, L"【先攻・後攻決定ダイスロール中...】");
        }
        else if (m_firstTurnPhase == FirstTurnPhase::ShowingResult)
        {
            if (m_decidedFirstPlayer == 0)
            {
                swprintf_s(msgBuf, L"青:%d vs 赤:%d 【引き分け！再ロール】", m_p1ResultRoll, m_p2ResultRoll);
            }
            else
            {
                uint8_t myRole =
                    gameContext.networkManager.IsConnected() ? gameContext.networkManager.GetAssignedPlayerType() : 1;
                bool amIFirst = (m_decidedFirstPlayer == myRole);
                swprintf_s(msgBuf, L"青:%d vs 赤:%d  ->  %s", m_p1ResultRoll, m_p2ResultRoll,
                           amIFirst ? L"【あなたが先攻！】" : L"【あいてが先攻！】");
            }
        }
        gameContext.debugRenderer.DrawText({screenWidth * 0.5f - 160.0f, 30.0f}, Vector4(1.0f, 0.9f, 0.2f, 1.0f),
                                           msgBuf);
        return;
    }

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

    // ログウィンドウ描画（手数・陣地塗りマス数のステータスバー付き）
    m_logManager.Render(context, gameContext.commonStates, gameContext.debugRenderer, m_colorPrimitiveBatch.get(),
                        m_colorEffect.get(), screenWidth, screenHeight, m_gameLogic.GetCurrentTurnCount(),
                        DiceBattleConstants::MAX_TURNS, m_gameLogic.GetPaintedTileCount(PlayerType::Player1),
                        m_gameLogic.GetPaintedTileCount(PlayerType::Player2));

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