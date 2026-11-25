#include "pch.h"
#include "OriginalGamePlayScene.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

bool OriginalGamePlayScene::IsMyTurn(GameContext& gameContext) const
{
    if (!gameContext.networkManager.IsConnected())
    {
        return true;
    }

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    uint8_t assignedRole = gameContext.networkManager.GetAssignedPlayerType();

    if (assignedRole == 1)
    {
        return (currentTurn == PlayerType::Player1);
    }
    else if (assignedRole == 2)
    {
        return (currentTurn == PlayerType::Player2);
    }

    return false;
}

void OriginalGamePlayScene::OnModeEnter(GameContext& gameContext)
{
    if (gameContext.networkManager.IsConnected())
    {
        uint8_t assignedRole = gameContext.networkManager.GetAssignedPlayerType();
        if (assignedRole == 1)
        {
            AddLog(L"【1P(青) / HOST】として接続（ホスト中心同期モード）");
        }
        else if (assignedRole == 2)
        {
            AddLog(L"【2P(赤) / GUEST】として接続（ホスト中心同期モード）");
        }
    }
}

void OriginalGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    (void)dt;

    // 自分がコマを動かして残り歩数が0になり、移動アニメも完了したらターン終了を通知
    if (gameContext.networkManager.IsConnected() && m_movementController.HasMovedThisTurn() &&
        m_gameLogic.GetRemainingSteps() <= 0 && !m_gameLogic.IsAnyPieceMoving())
    {
        // ホスト(1P)なら自分の行動完了後の確定盤面も2Pへ配信
        if (gameContext.networkManager.GetAssignedPlayerType() == 1)
        {
            SendHostWorldSync(gameContext);
        }

        gameContext.networkManager.SendEndTurn();

        // 自分のローカル盤面がまだ自分のターンのままなら即座に相手ターンへ切り替える
        if (IsMyTurn(gameContext))
        {
            m_gameLogic.ForceEndTurn();
        }
        ResetMoveState();
    }

    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        m_movementController.Reset();
    }
}

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