#pragma once
#pragma once

#include <vector>
#include <cstdint>
#include <memory>

class GameRoom;

// パケット処理の抽象基底クラス (コマンドパターン)
class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    // パケット受信時に実行される処理関数
    // 引数 clientId: 送信元クライアントID, room: 所属対戦部屋の参照, body: パケットのバイナリデータ
    virtual void Handle(uint32_t clientId, std::shared_ptr<GameRoom> room, const std::vector<uint8_t>& body) = 0;
};