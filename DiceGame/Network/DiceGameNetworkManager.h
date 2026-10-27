#pragma once
#include "NetworkManager.h"
#include <functional>

// ============================================================================
// DiceGame 専用のネットワーク制御派生クラス
// ============================================================================
class DiceGameNetworkManager : public NetworkManager
{
public:
    DiceGameNetworkManager() = default;
    ~DiceGameNetworkManager() override = default;

    // --- 送信用ヘルパー関数 ---

    // ダイスの出目を通信相手に送信
    bool SendDiceResult(int steps)
    {
        MsgDiceResult msg{};
        msg.header.size = sizeof(MsgDiceResult);
        msg.header.type = PacketType::CS_DiceResult;
        msg.steps = steps;
        return SendPacket(&msg, sizeof(msg));
    }

    // コマの移動（移動元と移動先座標）を通信相手に送信
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

    // コールバック関数型定義
    using OnDiceResultCallback = std::function<void(int steps)>;
    using OnMovePieceCallback = std::function<void(int fromX, int fromY, int toX, int toY)>;
    using OnWorldSyncCallback = std::function<void(const MsgWorldSync& syncPkt)>;

    // イベント受信ハンドラーの登録
    void SetOnDiceResult(OnDiceResultCallback cb)
    {
        m_onDiceResult = cb;
    }
    void SetOnMovePiece(OnMovePieceCallback cb)
    {
        m_onMovePiece = cb;
    }
    void SetOnWorldSync(OnWorldSyncCallback cb)
    {
        m_onWorldSync = cb;
    }

    // 受信パケットの安全な一括処理（毎フレーム Update の後に呼び出します）
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
                // ダイス出目パケットの受信処理
                case PacketType::CS_DiceResult:
                    if (m_onDiceResult && packet.size() >= sizeof(MsgDiceResult))
                    {
                        auto* msg = reinterpret_cast<MsgDiceResult*>(packet.data());
                        m_onDiceResult(msg->steps);
                    }
                    break;

                // コマ移動パケットの受信処理
                case PacketType::CS_MovePiece:
                    if (m_onMovePiece && packet.size() >= sizeof(MsgMovePiece))
                    {
                        auto* msg = reinterpret_cast<MsgMovePiece*>(packet.data());
                        m_onMovePiece(msg->fromX, msg->fromY, msg->toX, msg->toY);
                    }
                    break;

                // 盤面全体同期パケットの受信処理
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
    OnDiceResultCallback m_onDiceResult; // 出目受信コールバック
    OnMovePieceCallback m_onMovePiece;   // 移動受信コールバック
    OnWorldSyncCallback m_onWorldSync;   // 盤面同期受信コールバック
};