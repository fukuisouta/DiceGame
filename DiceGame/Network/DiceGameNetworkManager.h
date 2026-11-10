#pragma once
#include "NetworkManager.h"
#include <functional>

// DiceGame 専用のネットワーク制御派生クラス
class DiceGameNetworkManager : public NetworkManager
{
public:
    DiceGameNetworkManager() = default;
    ~DiceGameNetworkManager() override = default;

    // --- 送信用ヘルパー関数 ---

    // ダイスの出目を通信相手に送信する関数
    // 引数 steps: 判定された歩数
    // 戻り値: 送信成功時 true, 失敗時 false
    bool SendDiceResult(int steps)
    {
        MsgDiceResult msg{};
        msg.header.size = sizeof(MsgDiceResult);
        msg.header.type = PacketType::CS_DiceResult;
        msg.steps = steps;
        return SendPacket(&msg, sizeof(msg));
    }

    // コマの移動（移動元と移動先座標）を通信相手に送信する関数
    // 引数 fromX, fromY: 移動元座標, toX, toY: 移動先座標
    // 戻り値: 送信成功時 true, 失敗時 false
    bool SendMovePiece(int fromX, int fromY, int toX, int toY)
    {
        MsgMovePiece msg{};
        msg.header.size = sizeof(MsgMovePiece);
        msg.header.type = PacketType::CS_MovePiece;
        msg.fromX = fromX;
        msg.fromY = fromY;
        msg.toX = toX;
        msg.toY = toY;
        return SendPacket(&msg, sizeof(msg));
    }

    // --- 受信ハンドリング処理 ---

    using OnDiceResultCallback = std::function<void(int steps)>; // ダイス出目受信のコールバック型定義
    using OnMovePieceCallback =
        std::function<void(int fromX, int fromY, int toX, int toY)>; // コマ移動受信のコールバック型定義
    using OnWorldSyncCallback =
        std::function<void(const MsgWorldSync& syncPkt)>; // 盤面全体同期受信のコールバック型定義

    // 出目受信コールバック関数をセットする関数
    void SetOnDiceResult(OnDiceResultCallback cb)
    {
        m_onDiceResult = cb;
    }

    // 移動受信コールバック関数をセットする関数
    void SetOnMovePiece(OnMovePieceCallback cb)
    {
        m_onMovePiece = cb;
    }

    // 盤面同期受信コールバック関数をセットする関数
    void SetOnWorldSync(OnWorldSyncCallback cb)
    {
        m_onWorldSync = cb;
    }

    // 受信パケットの安全な一括解析およびコールバック実行を行う関数
    void DispatchPackets()
    {
        std::vector<uint8_t> packet;
        while (PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

            switch (header->type)
            {
                // ダイス出目パケットの処理
                case PacketType::CS_DiceResult:
                    if (m_onDiceResult && packet.size() >= sizeof(MsgDiceResult))
                    {
                        auto* msg = reinterpret_cast<MsgDiceResult*>(packet.data());
                        m_onDiceResult(msg->steps);
                    }
                    break;

                // コマ移動パケットの処理
                case PacketType::CS_MovePiece:
                    if (m_onMovePiece && packet.size() >= sizeof(MsgMovePiece))
                    {
                        auto* msg = reinterpret_cast<MsgMovePiece*>(packet.data());
                        m_onMovePiece(msg->fromX, msg->fromY, msg->toX, msg->toY);
                    }
                    break;

                // 盤面全体同期パケットの処理
                case PacketType::SC_WorldSync:
                    if (m_onWorldSync && packet.size() >= sizeof(MsgWorldSync))
                    {
                        auto* msg = reinterpret_cast<MsgWorldSync*>(packet.data());
                        m_onWorldSync(*msg);
                    }
                    break;

                default:
                    break;
            }
        }
    }

private:
    OnDiceResultCallback m_onDiceResult; // ダイス出目受信時に実行されるイベント通知関数
    OnMovePieceCallback m_onMovePiece;   // コマ移動受信時に実行されるイベント通知関数
    OnWorldSyncCallback m_onWorldSync;   // 盤面同期受信時に実行されるイベント通知関数
};