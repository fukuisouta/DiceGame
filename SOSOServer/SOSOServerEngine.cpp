#include "SOSOServerEngine.h"
#include "NetworkUtils.h"
#include "PacketType.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>

#ifndef _WIN32
#include <csignal>
#endif

namespace
{
    SOSOServerEngine* g_engineInstance = nullptr;

    void SafeCopyString(char* dest, size_t destSize, const std::string& src)
    {
        if (destSize == 0) return;
        size_t maxCopy = destSize - 1;
        size_t copyLen = (src.size() < maxCopy) ? src.size() : maxCopy;
        std::memcpy(dest, src.data(), copyLen);
        dest[copyLen] = '\0';
    }
}

SOSOServerEngine::~SOSOServerEngine()
{
    Shutdown();
}

#ifdef _WIN32
BOOL WINAPI SOSOServerEngine::ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    {
        int result = MessageBoxA(
            NULL,
            "SOSOServer を終了しますか？\n実行中の対戦セッションはすべて切断されます。",
            "SOSOServer 終了確認",
            MB_YESNO | MB_ICONWARNING | MB_TOPMOST
        );
        return (result == IDYES) ? FALSE : TRUE;
    }
    default:
        return FALSE;
    }
}
#else
void SOSOServerEngine::PosixSignalHandler(int signum)
{
    std::cout << "\n[SOSOServer] シグナル (" << signum << ") を受信しました。安全に停止します..." << std::endl;
    if (g_engineInstance)
    {
        g_engineInstance->Shutdown();
    }
}
#endif

bool SOSOServerEngine::Initialize(const ServerNetworkConfig& config)
{
    m_config = config;
    g_engineInstance = this;

#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
    std::signal(SIGINT, PosixSignalHandler);
    std::signal(SIGTERM, PosixSignalHandler);
#endif

    m_network.SetOnConnectCallback([this](uint32_t clientId) { OnClientConnected(clientId); });
    m_network.SetOnDisconnectCallback([this](uint32_t clientId) { OnClientDisconnected(clientId); });

    if (!m_network.Start(m_config.port))
    {
        std::cerr << "[FATAL] SOSOServer の通信開始に失敗しました。Port: " << m_config.port << std::endl;
        return false;
    }

    m_isInitialized = true;

    std::string primaryIp = NetworkUtils::GetPrimaryLocalIP();
    std::cout << "===========================================" << std::endl;
    std::cout << "  SOSOServer Dedicated Game Server Ready  " << std::endl;
    std::cout << "  Local IP Address : " << primaryIp << std::endl;
    std::cout << "  Port             : " << m_config.port << std::endl;
    std::cout << "===========================================" << std::endl;

    return true;
}

void SOSOServerEngine::OnClientConnected(uint32_t clientId)
{
    std::cout << "[SOSOServer] クライアント " << clientId << " がロビーに接続しました。" << std::endl;
}

void SOSOServerEngine::OnClientDisconnected(uint32_t clientId)
{
    std::cout << "[SOSOServer] クライアント " << clientId << " が切断。" << std::endl;
    m_roomManager.RemovePlayerFromRoom(clientId);
}

void SOSOServerEngine::Run()
{
    if (!m_isInitialized) return;

    const auto frameDuration = std::chrono::milliseconds(1000 / m_config.targetFps);

    while (m_isInitialized)
    {
        auto startTime = std::chrono::steady_clock::now();

        m_network.Update();

        PacketMessage msg;
        while (m_network.PopPacket(msg))
        {
            // 1. ルーム作成要求
            if (msg.packetType == static_cast<uint16_t>(PacketType::CS_CreateRoom) &&
                msg.body.size() >= sizeof(MsgCreateRoom))
            {
                const auto* pkt = reinterpret_cast<const MsgCreateRoom*>(msg.body.data());
                char safePw[4] = {};
                std::memcpy(safePw, pkt->password, 3);
                char safeName[32] = {};
                std::memcpy(safeName, pkt->hostName, 31);

                auto room = m_roomManager.CreateRoom(msg.clientId, pkt->gameMode, (pkt->isPrivate != 0), safePw, safeName);

                std::cout << "[Room #" << room->GetRoomId() << "] ルーム作成 Host:" << safeName
                    << " Mode:" << (pkt->gameMode == 1 ? "Advance" : "Original")
                    << " Type:" << (pkt->isPrivate ? "Private" : "Free") << std::endl;

                MsgRoomCreated res{};
                res.header.size = sizeof(MsgRoomCreated);
                res.header.packetType = static_cast<uint16_t>(PacketType::SC_RoomCreated);
                res.roomId = room->GetRoomId();
                m_network.SendPacket(msg.clientId, &res, sizeof(res));
                continue;
            }

            // 2. ルーム待機キャンセル
            if (msg.packetType == static_cast<uint16_t>(PacketType::CS_CancelRoom))
            {
                m_roomManager.RemovePlayerFromRoom(msg.clientId);
                std::cout << "[SOSOServer] Client " << msg.clientId << " がルームを解散しました。" << std::endl;
                continue;
            }

            // 3. 募集中ルーム一覧の取得要求
            if (msg.packetType == static_cast<uint16_t>(PacketType::CS_RequestRoomList))
            {
                auto waitingRooms = m_roomManager.GetWaitingRooms();
                MsgRoomList listPkt{};
                listPkt.header.size = sizeof(MsgRoomList);
                listPkt.header.packetType = static_cast<uint16_t>(PacketType::SC_RoomList);

                uint8_t count = 0;
                for (const auto& r : waitingRooms)
                {
                    if (count >= MAX_LISTED_ROOMS) break;
                    listPkt.rooms[count].roomId = r->GetRoomId();
                    listPkt.rooms[count].gameMode = r->GetGameMode();
                    listPkt.rooms[count].isPrivate = r->IsPrivate() ? 1 : 0;
                    SafeCopyString(listPkt.rooms[count].hostName, sizeof(listPkt.rooms[count].hostName), r->GetHostName());
                    count++;
                }
                listPkt.roomCount = count;
                m_network.SendPacket(msg.clientId, &listPkt, sizeof(listPkt));
                continue;
            }

            // 4. ルーム参加要求
            if (msg.packetType == static_cast<uint16_t>(PacketType::CS_JoinRoom) &&
                msg.body.size() >= sizeof(MsgJoinRoom))
            {
                const auto* joinPkt = reinterpret_cast<const MsgJoinRoom*>(msg.body.data());
                char safePw[4] = {};
                std::memcpy(safePw, joinPkt->password, 3);
                char safeGuest[32] = {};
                std::memcpy(safeGuest, joinPkt->guestName, 31);

                std::shared_ptr<GameRoom> joinedRoom;
                uint8_t reason = m_roomManager.JoinRoom(msg.clientId, joinPkt->roomId, safePw, safeGuest, joinedRoom);

                MsgJoinRoomResult resPkt{};
                resPkt.header.size = sizeof(MsgJoinRoomResult);
                resPkt.header.packetType = static_cast<uint16_t>(PacketType::SC_JoinRoomResult);
                resPkt.success = (reason == 0) ? 1 : 0;
                resPkt.reason = reason;
                m_network.SendPacket(msg.clientId, &resPkt, sizeof(resPkt));

                if (reason == 0 && joinedRoom)
                {
                    std::cout << "-------------------------------------------" << std::endl;
                    std::cout << "  [MATCH START] Room #" << joinedRoom->GetRoomId() << " 対戦開始！" << std::endl;
                    std::cout << "  1P: " << joinedRoom->GetHostName() << " vs 2P: " << joinedRoom->GetGuestName() << std::endl;
                    std::cout << "-------------------------------------------" << std::endl;

                    MsgMatchStart msgP1{};
                    msgP1.header.size = sizeof(MsgMatchStart);
                    msgP1.header.packetType = static_cast<uint16_t>(PacketType::SC_MatchStart);
                    msgP1.assignedPlayerType = 1;
                    msgP1.gameMode = joinedRoom->GetGameMode();
                    SafeCopyString(msgP1.opponentName, sizeof(msgP1.opponentName), joinedRoom->GetGuestName());
                    m_network.SendPacket(joinedRoom->GetPlayer1Id(), &msgP1, sizeof(msgP1));

                    MsgMatchStart msgP2{};
                    msgP2.header.size = sizeof(MsgMatchStart);
                    msgP2.header.packetType = static_cast<uint16_t>(PacketType::SC_MatchStart);
                    msgP2.assignedPlayerType = 2;
                    msgP2.gameMode = joinedRoom->GetGameMode();
                    SafeCopyString(msgP2.opponentName, sizeof(msgP2.opponentName), joinedRoom->GetHostName());
                    m_network.SendPacket(joinedRoom->GetPlayer2Id(), &msgP2, sizeof(msgP2));
                }
                continue;
            }

            // 以下、ルーム入室後のゲーム内パケット処理
            auto room = m_roomManager.GetRoomByClientId(msg.clientId);
            if (!room) continue;

            if (msg.packetType == static_cast<uint16_t>(PacketType::CS_FirstTurnRoll) &&
                msg.body.size() >= sizeof(MsgFirstTurnRoll))
            {
                const auto* rollPkt = reinterpret_cast<const MsgFirstTurnRoll*>(msg.body.data());
                room->SetFirstTurnRoll(msg.clientId, rollPkt->rollValue);

                if (room->AreBothFirstRollsReady())
                {
                    int32_t p1Val = room->GetP1FirstRoll();
                    int32_t p2Val = room->GetP2FirstRoll();
                    uint8_t firstPlayer = (p1Val > p2Val) ? 1 : ((p2Val > p1Val) ? 2 : 0);

                    MsgFirstTurnResult resPkt{};
                    resPkt.header.size = sizeof(MsgFirstTurnResult);
                    resPkt.header.packetType = static_cast<uint16_t>(PacketType::SC_FirstTurnResult);
                    resPkt.p1Roll = p1Val;
                    resPkt.p2Roll = p2Val;
                    resPkt.firstPlayer = firstPlayer;

                    m_network.SendPacket(room->GetPlayer1Id(), &resPkt, sizeof(resPkt));
                    m_network.SendPacket(room->GetPlayer2Id(), &resPkt, sizeof(resPkt));

                    room->ResetFirstTurnRolls();
                }
                continue;
            }

            uint32_t opponentId = room->GetOpponentId(msg.clientId);
            if (opponentId != 0 && !msg.body.empty())
            {
                m_network.SendPacket(opponentId, msg.body.data(), msg.body.size());
            }
        }

        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        if (elapsedTime < frameDuration)
        {
            std::this_thread::sleep_for(frameDuration - elapsedTime);
        }
    }
}

void SOSOServerEngine::Shutdown()
{
    bool expected = true;
    if (!m_isInitialized.compare_exchange_strong(expected, false)) return;

    m_network.Stop();
    std::cout << "[SOSOServer] サーバーを停止しました。" << std::endl;
}