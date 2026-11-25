#pragma once
#include "NetworkManager.h"
#include "NetworkPackets.h"
#include <functional>
#include <string>
#include <vector>
#include <cstring>

// DiceGame 専用のネットワーク制御派生クラス
class DiceGameNetworkManager : public NetworkManager
{
public:
    DiceGameNetworkManager() = default;
    ~DiceGameNetworkManager() override = default;

    // --- ロビー・プレイヤー情報・ルーム管理用ヘルパー ---

    void SetLocalPlayerName(const std::string& name)
    {
        m_localPlayerName = name.empty() ? "Player" : name;
    }

    const std::string& GetLocalPlayerName() const
    {
        return m_localPlayerName;
    }

    void SetOpponentName(const std::string& name)
    {
        m_opponentName = name;
    }

    const std::string& GetOpponentName() const
    {
        return m_opponentName;
    }

    void SendCreateRoom(uint8_t gameMode, bool isPrivate, const std::string& password)
    {
        MsgCreateRoom pkt{};
        pkt.header.size = sizeof(MsgCreateRoom);
        pkt.header.type = PacketType::CS_CreateRoom;
        pkt.gameMode = gameMode;
        pkt.isPrivate = isPrivate ? 1 : 0;
        strncpy_s(pkt.password, sizeof(pkt.password), password.c_str(), _TRUNCATE);
        strncpy_s(pkt.hostName, sizeof(pkt.hostName), m_localPlayerName.c_str(), _TRUNCATE);
        SendPacket(&pkt, sizeof(pkt));
    }

    void SendCancelRoom()
    {
        PacketHeader pkt{};
        pkt.size = sizeof(PacketHeader);
        pkt.type = PacketType::CS_CancelRoom;
        SendPacket(&pkt, sizeof(pkt));
    }

    void SendRequestRoomList()
    {
        PacketHeader pkt{};
        pkt.size = sizeof(PacketHeader);
        pkt.type = PacketType::CS_RequestRoomList;
        SendPacket(&pkt, sizeof(pkt));
    }

    void SendJoinRoom(uint32_t roomId, const std::string& password)
    {
        MsgJoinRoom pkt{};
        pkt.header.size = sizeof(MsgJoinRoom);
        pkt.header.type = PacketType::CS_JoinRoom;
        pkt.roomId = roomId;
        strncpy_s(pkt.password, sizeof(pkt.password), password.c_str(), _TRUNCATE);
        strncpy_s(pkt.guestName, sizeof(pkt.guestName), m_localPlayerName.c_str(), _TRUNCATE);
        SendPacket(&pkt, sizeof(pkt));
    }

    // --- ゲーム内送信用ヘルパー関数 ---

    bool SendFirstTurnRoll(int rollValue)
    {
        MsgFirstTurnRoll msg{};
        msg.header.size = sizeof(MsgFirstTurnRoll);
        msg.header.type = PacketType::CS_FirstTurnRoll;
        msg.rollValue = rollValue;
        return SendPacket(&msg, sizeof(msg));
    }

    bool SendDiceResult(int steps)
    {
        MsgDiceResult msg{};
        msg.header.size = sizeof(MsgDiceResult);
        msg.header.type = PacketType::SC_DiceResult;
        msg.steps = steps;
        return SendPacket(&msg, sizeof(msg));
    }

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

    bool SendSkillUse(uint8_t skillType, int targetX = -1, int targetY = -1)
    {
        MsgSkillUse msg{};
        msg.header.size = sizeof(MsgSkillUse);
        msg.header.type = PacketType::CS_SkillUse;
        msg.skillType = skillType;
        msg.targetX = targetX;
        msg.targetY = targetY;
        return SendPacket(&msg, sizeof(msg));
    }

    bool SendEndTurn()
    {
        PacketHeader msg{};
        msg.size = sizeof(PacketHeader);
        msg.type = PacketType::CS_EndTurn;
        return SendPacket(&msg, sizeof(msg));
    }

    // --- 受信ハンドリング処理 ---

    using OnMatchStartCallback = std::function<void(uint8_t assignedPlayerType)>;
    using OnFirstTurnResultCallback = std::function<void(int p1Roll, int p2Roll, uint8_t firstPlayer)>;
    using OnDiceResultCallback = std::function<void(int steps)>;
    using OnMovePieceCallback = std::function<void(int fromX, int fromY, int toX, int toY)>;
    using OnEndTurnCallback = std::function<void()>;
    using OnWorldSyncCallback = std::function<void(const MsgWorldSync& syncPkt)>;
    using OnSkillUseCallback = std::function<void(uint8_t skillType, int targetX, int targetY)>;

    void SetOnMatchStart(OnMatchStartCallback cb)
    {
        m_onMatchStart = cb;
    }
    void SetOnFirstTurnResult(OnFirstTurnResultCallback cb)
    {
        m_onFirstTurnResult = cb;
    }
    void SetOnDiceResult(OnDiceResultCallback cb)
    {
        m_onDiceResult = cb;
    }
    void SetOnMovePiece(OnMovePieceCallback cb)
    {
        m_onMovePiece = cb;
    }
    void SetOnEndTurn(OnEndTurnCallback cb)
    {
        m_onEndTurn = cb;
    }
    void SetOnWorldSync(OnWorldSyncCallback cb)
    {
        m_onWorldSync = cb;
    }
    void SetOnSkillUse(OnSkillUseCallback cb)
    {
        m_onSkillUse = cb;
    }

    // 受信パケットの安全な一括解析およびコールバック実行を行う関数
    void DispatchPackets() override
    {
        Update();

        std::vector<uint8_t> packet;
        while (PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;

            const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packet.data());

            if (header->type == PacketType::SC_MatchStart)
            {
                if (packet.size() >= sizeof(MsgMatchStart))
                {
                    const auto* msg = reinterpret_cast<const MsgMatchStart*>(packet.data());
                    m_assignedPlayerType = msg->assignedPlayerType;
                    m_opponentName = msg->opponentName;
                    if (m_onMatchStart)
                    {
                        m_onMatchStart(m_assignedPlayerType);
                    }
                }
            }
            else if (header->type == PacketType::SC_FirstTurnResult)
            {
                if (m_onFirstTurnResult && packet.size() >= sizeof(MsgFirstTurnResult))
                {
                    const auto* msg = reinterpret_cast<const MsgFirstTurnResult*>(packet.data());
                    m_onFirstTurnResult(msg->p1Roll, msg->p2Roll, msg->firstPlayer);
                }
            }
            else if (header->type == PacketType::SC_DiceResult)
            {
                if (m_onDiceResult && packet.size() >= sizeof(MsgDiceResult))
                {
                    const auto* msg = reinterpret_cast<const MsgDiceResult*>(packet.data());
                    m_onDiceResult(msg->steps);
                }
            }
            else if (header->type == PacketType::CS_MovePiece)
            {
                if (m_onMovePiece && packet.size() >= sizeof(MsgMovePiece))
                {
                    const auto* msg = reinterpret_cast<const MsgMovePiece*>(packet.data());
                    m_onMovePiece(msg->fromX, msg->fromY, msg->toX, msg->toY);
                }
            }
            else if (header->type == PacketType::CS_EndTurn)
            {
                if (m_onEndTurn)
                {
                    m_onEndTurn();
                }
            }
            else if (header->type == PacketType::CS_SkillUse)
            {
                if (m_onSkillUse && packet.size() >= sizeof(MsgSkillUse))
                {
                    const auto* msg = reinterpret_cast<const MsgSkillUse*>(packet.data());
                    m_onSkillUse(msg->skillType, msg->targetX, msg->targetY);
                }
            }
            else if (header->type == PacketType::SC_WorldSync)
            {
                if (m_onWorldSync && packet.size() >= sizeof(MsgWorldSync))
                {
                    const auto* msg = reinterpret_cast<const MsgWorldSync*>(packet.data());
                    m_onWorldSync(*msg);
                }
            }
        }
    }

private:
    OnMatchStartCallback m_onMatchStart;
    OnFirstTurnResultCallback m_onFirstTurnResult;
    OnDiceResultCallback m_onDiceResult;
    OnMovePieceCallback m_onMovePiece;
    OnEndTurnCallback m_onEndTurn;
    OnWorldSyncCallback m_onWorldSync;
    OnSkillUseCallback m_onSkillUse;

    std::string m_localPlayerName = "Player";
    std::string m_opponentName = "Opponent";
};