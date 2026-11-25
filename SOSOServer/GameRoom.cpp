#include "GameRoom.h"

bool GameRoom::AddPlayer(uint32_t clientId)
{
    if (m_player1Id == 0)
    {
        m_player1Id = clientId;
        m_p1FirstRoll = 0;
        m_state = RoomState::WaitingForPlayers;
        return true;
    }
    else if (m_player2Id == 0)
    {
        m_player2Id = clientId;
        m_p1FirstRoll = 0;
        m_p2FirstRoll = 0;
        m_state = RoomState::Playing;
        m_gameLogic.Initialize();
        return true;
    }
    return false;
}

void GameRoom::RemovePlayer(uint32_t clientId)
{
    if (m_player1Id == clientId)
    {
        m_player1Id = 0;
        m_p1FirstRoll = 0;
    }
    else if (m_player2Id == clientId)
    {
        m_player2Id = 0;
        m_p2FirstRoll = 0;
    }

    if (m_player1Id == 0 && m_player2Id == 0) m_state = RoomState::Finished;
    else m_state = RoomState::WaitingForPlayers;
}

void GameRoom::SetFirstTurnRoll(uint32_t clientId, int32_t value)
{
    if (clientId == m_player1Id)
    {
        m_p1FirstRoll = value;
    }
    else if (clientId == m_player2Id)
    {
        m_p2FirstRoll = value;
    }
}

ServerPlayerType GameRoom::GetPlayerType(uint32_t clientId) const
{
    if (clientId == m_player1Id) return ServerPlayerType::Player1;
    if (clientId == m_player2Id) return ServerPlayerType::Player2;
    return ServerPlayerType::None;
}

uint32_t GameRoom::GetOpponentId(uint32_t clientId) const
{
    if (clientId == m_player1Id) return m_player2Id;
    if (clientId == m_player2Id) return m_player1Id;
    return 0;
}