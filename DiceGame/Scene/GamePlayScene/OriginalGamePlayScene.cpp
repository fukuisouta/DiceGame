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

    // ダイス出目受信処理
    gameContext.networkManager.SetOnDiceResult(
        [this](int steps)
        {
            m_gameLogic.StartTurnWithSteps(steps);
            ResetMoveState();
            AddLog(L"あいてがダイスを振り【%d】が出ました", steps);
        });

    // コマ移動受信処理
    gameContext.networkManager.SetOnMovePiece([this, &gameContext](int fX, int fY, int tX, int tY)
                                              { ExecuteMove(fX, fY, tX, tY, true, &gameContext); });

    // ★ クライアント側: ホストからのワールド姿勢データを受信して即時反映
    gameContext.networkManager.SetOnWorldSync([this](const MsgWorldSync& syncPkt)
                                              { m_gameLogic.ApplyWorldSyncFromHost(syncPkt); });
}

void OriginalGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    // クライアントで自分のターンでない場合はカーソル選択解除
    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        m_selectedX = -1;
        m_selectedY = -1;
    }

    // ターン終了パケットの送信
    if (gameContext.networkManager.IsConnected() && IsMyTurn(gameContext))
    {
        if (m_hasMovedThisTurn && m_gameLogic.GetRemainingSteps() <= 0 && !m_gameLogic.IsAnyPieceMoving())
        {
            PacketHeader msg{};
            msg.size = sizeof(PacketHeader);
            msg.type = PacketType::CS_EndTurn;
            gameContext.networkManager.SendPacket(&msg, sizeof(msg));

            m_hasMovedThisTurn = false;
        }
    }

    // ★ ホスト側: 毎フレーム、ホスト上の全オブジェクトの絶対姿勢を構築してクライアントへ一括送信
    if (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
    {
        MsgWorldSync syncPkt{};
        m_gameLogic.BuildWorldSyncPacket(syncPkt);
        gameContext.networkManager.SendPacket(&syncPkt, sizeof(syncPkt));
    }
}

void OriginalGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                           GameContext& gameContext)
{
    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        gameContext.debugRenderer.DrawText({screenWidth * 0.5f - 110.0f, 20.0f},
                                           DirectX::SimpleMath::Vector4(1.0f, 0.8f, 0.2f, 1.0f),
                                           L"【あいての思考・操作中...】");
    }
}