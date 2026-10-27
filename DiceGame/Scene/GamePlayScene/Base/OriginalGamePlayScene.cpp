#include "pch.h"
#include "OriginalGamePlayScene.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 接続モード（Host / Client）およびターン管理をもとに自分のターンか判定
bool OriginalGamePlayScene::IsMyTurn(GameContext& gameContext) const
{
    if (!gameContext.networkManager.IsConnected())
    {
        return true; // ローカル単体プレイ時は常時操作可能
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

// オンライン対戦用コールバックの設定およびモード開始処理
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

    // ダイス出目受信処理（通信相手の振った出目を反映）
    gameContext.networkManager.SetOnDiceResult(
        [this](int steps)
        {
            m_gameLogic.StartTurnWithSteps(steps);
            ResetMoveState();
            AddLog(L"あいてがダイスを振り【%d】が出ました", steps);
        });

    // コマ移動パケット受信処理（PieceMovementController 経由で相手の移動を反映）
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

    // クライアント側: ホストから送信されたワールド全体姿勢（3D座標・回転）の即時同期
    gameContext.networkManager.SetOnWorldSync([this](const MsgWorldSync& syncPkt)
                                              { m_gameLogic.ApplyWorldSyncFromHost(syncPkt); });
}

// 毎フレームのターン状態チェック・パケット同期送信
void OriginalGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    // クライアントで自分の手番でない場合は選択中のコマを強制解除
    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        m_movementController.Reset();
    }

    // ターン終了パケットの送信（自分の行動完了時）
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

    // ホスト側: 毎フレーム、ホスト上の全オブジェクト絶対姿勢を構築してクライアントへ一括同期送信
    if (gameContext.networkManager.IsConnected() && gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
    {
        MsgWorldSync syncPkt{};
        m_gameLogic.BuildWorldSyncPacket(syncPkt);
        gameContext.networkManager.SendPacket(&syncPkt, sizeof(syncPkt));
    }
}

// 相手のターン中のメッセージオーバーレイ描画
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