#pragma once

#include "GameRoom.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

class RoomManager
{
public:
    RoomManager() = default;
    ~RoomManager() = default;

    // 指定条件で新規ルームを作成しホストを1Pとして入室させる
    std::shared_ptr<GameRoom> CreateRoom(uint32_t hostClientId, uint8_t gameMode, bool isPrivate,
        const std::string& password, const std::string& hostName);

    // 指定されたルームIDへ参加させる（戻り値: 0=成功, 1=満員/存在しない, 2=パスワード不一致）
    uint8_t JoinRoom(uint32_t guestClientId, uint32_t roomId, const std::string& password,
        const std::string& guestName, std::shared_ptr<GameRoom>& outRoom);

    // 待機中（参加者募集中）のルーム一覧を取得する
    std::vector<std::shared_ptr<GameRoom>> GetWaitingRooms() const;

    // クライアントを部屋から退出させ、空になった部屋を削除する
    void RemovePlayerFromRoom(uint32_t clientId);

    // クライアントが所属しているルームを取得する
    std::shared_ptr<GameRoom> GetRoomByClientId(uint32_t clientId) const;

private:
    uint32_t m_nextRoomId = 1;
    std::unordered_map<uint32_t, std::shared_ptr<GameRoom>> m_rooms;
    std::unordered_map<uint32_t, uint32_t> m_clientToRoomId;
};