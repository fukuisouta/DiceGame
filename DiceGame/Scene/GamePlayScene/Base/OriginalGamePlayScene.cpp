#include "pch.h"
#include "OriginalGamePlayScene.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 自分の手番判定
bool OriginalGamePlayScene::IsMyTurn(GameContext& gameContext) const
{
    if (!gameContext.networkManager.IsConnected())
    {
        return true;
    }

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
    {
        return (currentTurn == PlayerType::Player1);
    }
    else if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Client)
    {
        return (currentTurn == PlayerType::Player2);
    }

    return false;
}

// モード進入時処理
void OriginalGamePlayScene::OnModeEnter(GameContext& gameContext)
{
    if (gameContext.networkManager.IsConnected())
    {
        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            AddLog(L"【1P】ホストとして接続（全描画集約モード）");
        }
        else
        {
            AddLog(L"【2P】クライアントとして接続（受信用表示モード）");
        }
    }

    gameContext.networkManager.SetOnDiceResult(
        [this](int steps)
        {
            m_gameLogic.StartTurnWithSteps(steps);
            ResetMoveState();
            AddLog(L"あいてがダイスを振り【%d】が出ました", steps);
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

    gameContext.networkManager.SetOnWorldSync([this](const MsgWorldSync& syncPkt)
                                              { m_gameLogic.ApplyWorldSyncFromHost(syncPkt); });
}

// モード更新処理
void OriginalGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    (void)dt;

    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        m_movementController.Reset();
    }

    if (gameContext.networkManager.IsConnected() && IsMyTurn(gameContext))
    {
        if (m_movementController.HasMovedThisTurn() && m_gameLogic.GetRemainingSteps() <= 0 &&
            !m_gameLogic.IsAnyPieceMoving())
        {
            PacketHeader msg{};
            msg.size = sizeof(PacketHeader);
            msg.type = PacketType::CS_EndTurn;
            gameContext.networkManager.SendPacket(&msg, sizeof(msg));

            ResetMoveState();
        }
    }

    if (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
    {
        MsgWorldSync syncPkt{};
        m_gameLogic.BuildWorldSyncPacket(syncPkt);
        gameContext.networkManager.SendPacket(&syncPkt, sizeof(syncPkt));
    }
}

// 2D描画処理
void OriginalGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                           GameContext& gameContext)
{
    (void)context;

    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        gameContext.debugRenderer.DrawText({screenWidth * 0.5f - OriginalSceneConstants::THINKING_TEXT_POS_X_OFFSET,
                                            OriginalSceneConstants::THINKING_TEXT_POS_Y},
                                           DirectX::SimpleMath::Vector4(1.0f, 0.8f, 0.2f, 1.0f),
                                           L"【あいての思考・操作中...】");
    }
}