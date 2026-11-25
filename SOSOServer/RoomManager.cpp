#include "RoomManager.h"

std::shared_ptr<GameRoom> RoomManager::CreateRoom(uint32_t hostClientId, uint8_t gameMode, bool isPrivate,
    const std::string& password, const std::string& hostName)
{
    // 既にどこかの部屋に入っていたら先に退出させる
    RemovePlayerFromRoom(hostClientId);

    uint32_t roomId = m_nextRoomId++;
    auto room = std::make_shared<GameRoom>(roomId);
    room->ConfigureRoom(gameMode, isPrivate, password, hostName);
    room->AddPlayer(hostClientId);

    m_rooms[roomId] = room;
    m_clientToRoomId[hostClientId] = roomId;
    return room;
}

uint8_t RoomManager::JoinRoom(uint32_t guestClientId, uint32_t roomId, const std::string& password,
    const std::string& guestName, std::shared_ptr<GameRoom>& outRoom)
{
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
    {
        return 1; // ルームが存在しない
    }

    auto room = it->second;
    if (room->GetState() != RoomState::WaitingForPlayers || room->GetPlayer2Id() != 0)
    {
        return 1; // 満員または既に開始済み
    }

    if (room->IsPrivate() && room->GetPassword() != password)
    {
        return 2; // パスワード不一致
    }

    // 既に別部屋にいたら退出
    RemovePlayerFromRoom(guestClientId);

    room->SetGuestName(guestName);
    if (!room->AddPlayer(guestClientId))
    {
        return 1;
    }

    m_clientToRoomId[guestClientId] = roomId;
    outRoom = room;
    return 0; // 成功
}

std::vector<std::shared_ptr<GameRoom>> RoomManager::GetWaitingRooms() const
{
    std::vector<std::shared_ptr<GameRoom>> result;
    for (const auto& pair : m_rooms)
    {
        if (pair.second->GetState() == RoomState::WaitingForPlayers && pair.second->GetPlayer1Id() != 0)
        {
            result.push_back(pair.second);
        }
    }
    return result;
}

void RoomManager::RemovePlayerFromRoom(uint32_t clientId)
{
    auto mapIt = m_clientToRoomId.find(clientId);
    if (mapIt == m_clientToRoomId.end())
        return;

    uint32_t roomId = mapIt->second;
    m_clientToRoomId.erase(mapIt);

    auto roomIt = m_rooms.find(roomId);
    if (roomIt != m_rooms.end())
    {
        auto room = roomIt->second;
        room->RemovePlayer(clientId);
        if (room->GetState() == RoomState::Finished || (room->GetPlayer1Id() == 0 && room->GetPlayer2Id() == 0))
        {
            m_rooms.erase(roomIt);
        }
    }
}

std::shared_ptr<GameRoom> RoomManager::GetRoomByClientId(uint32_t clientId) const
{
    auto mapIt = m_clientToRoomId.find(clientId);
    if (mapIt == m_clientToRoomId.end())
        return nullptr;

    auto roomIt = m_rooms.find(mapIt->second);
    if (roomIt == m_rooms.end())
        return nullptr;

    return roomIt->second;
}