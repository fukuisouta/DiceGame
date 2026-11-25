#include "ServerGameLogic.h"
#include <cmath>

void ServerGameLogic::Initialize(const BoardConfig& config)
{
    m_config = config;
    m_pieces.clear();
    m_currentTurn = ServerPlayerType::Player1;
    m_remainingSteps = 0;
    m_isGameOver = false;
    m_winner = ServerPlayerType::None;

    auto AddPiece = [this](ServerPlayerType owner, uint8_t type, int gx, int gy)
        {
            ServerBoardPiece p;
            p.owner = owner;
            p.type = type;
            p.gridX = static_cast<int8_t>(gx);
            p.gridY = static_cast<int8_t>(gy);
            p.isActive = true;
            p.topFace = 1;
            m_pieces.push_back(p);
        };

    // 2P初期配置
    AddPiece(ServerPlayerType::Player2, 1, 0, 0);
    AddPiece(ServerPlayerType::Player2, 1, 2, 0);
    AddPiece(ServerPlayerType::Player2, 0, 4, 0);
    AddPiece(ServerPlayerType::Player2, 1, 6, 0);
    AddPiece(ServerPlayerType::Player2, 1, 8, 0);

    // 1P初期配置
    AddPiece(ServerPlayerType::Player1, 1, 0, 7);
    AddPiece(ServerPlayerType::Player1, 1, 2, 7);
    AddPiece(ServerPlayerType::Player1, 0, 4, 7);
    AddPiece(ServerPlayerType::Player1, 1, 6, 7);
    AddPiece(ServerPlayerType::Player1, 1, 8, 7);
}

int ServerGameLogic::RollStepDice()
{
    std::uniform_int_distribution<int> dist(m_config.minDiceValue, m_config.maxDiceValue);
    m_remainingSteps = dist(m_rng);
    return m_remainingSteps;
}

bool ServerGameLogic::ValidateAndExecuteMove(ServerPlayerType player, int fromX, int fromY, int toX, int toY)
{
    if (m_isGameOver || player != m_currentTurn || m_remainingSteps <= 0) return false;

    int attackerIdx = -1;
    for (size_t i = 0; i < m_pieces.size(); ++i)
    {
        if (m_pieces[i].isActive && m_pieces[i].gridX == fromX && m_pieces[i].gridY == fromY && m_pieces[i].owner == player)
        {
            attackerIdx = static_cast<int>(i);
            break;
        }
    }
    if (attackerIdx == -1) return false;

    if (toX < 0 || toX >= m_config.width || toY < 0 || toY >= m_config.height) return false;
    if ((std::abs(toX - fromX) + std::abs(toY - fromY)) != 1) return false;

    int defenderIdx = -1;
    for (size_t i = 0; i < m_pieces.size(); ++i)
    {
        if (m_pieces[i].isActive && m_pieces[i].gridX == toX && m_pieces[i].gridY == toY)
        {
            defenderIdx = static_cast<int>(i);
            break;
        }
    }

    if (defenderIdx != -1 && m_pieces[defenderIdx].owner == player) return false;

    if (defenderIdx != -1)
    {
        ResolveBattle(static_cast<size_t>(attackerIdx), static_cast<size_t>(defenderIdx));
    }
    else
    {
        m_pieces[attackerIdx].gridX = static_cast<int8_t>(toX);
        m_pieces[attackerIdx].gridY = static_cast<int8_t>(toY);
    }

    m_remainingSteps--;
    if (m_remainingSteps <= 0) SwitchTurn();

    return true;
}

void ServerGameLogic::ResolveBattle(size_t attackerIdx, size_t defenderIdx)
{
    auto& attacker = m_pieces[attackerIdx];
    auto& defender = m_pieces[defenderIdx];

    if (defender.type == 0 || attacker.topFace >= defender.topFace)
    {
        defender.isActive = false;
        attacker.gridX = defender.gridX;
        attacker.gridY = defender.gridY;
    }
    else
    {
        attacker.isActive = false;
    }
}

void ServerGameLogic::SwitchTurn()
{
    m_currentTurn = (m_currentTurn == ServerPlayerType::Player1) ? ServerPlayerType::Player2 : ServerPlayerType::Player1;
    m_remainingSteps = 0;
}

bool ServerGameLogic::CheckGameOver()
{
    bool p1King = false, p2King = false;
    for (const auto& p : m_pieces)
    {
        if (p.isActive && p.type == 0)
        {
            if (p.owner == ServerPlayerType::Player1) p1King = true;
            if (p.owner == ServerPlayerType::Player2) p2King = true;
        }
    }

    if (!p1King) { m_isGameOver = true; m_winner = ServerPlayerType::Player2; return true; }
    if (!p2King) { m_isGameOver = true; m_winner = ServerPlayerType::Player1; return true; }
    return false;
}