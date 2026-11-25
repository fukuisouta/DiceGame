#pragma once


#include "IPacketHandler.h"
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <iostream>

// 受信パケットのタイプIDに応じて適切なハンドラーを実行するディスパッチャークラス
class PacketDispatcher
{
public:
    PacketDispatcher() = default;
    ~PacketDispatcher() = default;

    // パケットタイプに対応する処理ハンドラーを登録する関数
    // 引数 packetType: パケット識別ID, handler: 登録するハンドラーインスタンス
    void RegisterHandler(uint16_t packetType, std::shared_ptr<IPacketHandler> handler)
    {
        m_handlers[packetType] = handler;
    }

    // 受信パケットに対応するハンドラーを実行する関数
    // 引数 clientId: 送信元ID, room: 所属部屋, packetType: パケットID, body: バイナリデータ
    void Dispatch(uint32_t clientId, std::shared_ptr<GameRoom> room, uint16_t packetType, const std::vector<uint8_t>& body)
    {
        auto it = m_handlers.find(packetType);
        if (it != m_handlers.end() && it->second)
        {
            it->second->Handle(clientId, room, body);
        }
        else
        {
            std::cout << "[WARN] 未登録のパケットタイプを受信しました。ID: " << packetType << std::endl;
        }
    }

private:
    std::unordered_map<uint16_t, std::shared_ptr<IPacketHandler>> m_handlers; // ハンドラーマップ
};