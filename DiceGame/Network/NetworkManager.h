#pragma once

#include "pch.h"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "NetworkPackets.h"

namespace NetworkConstants
{
    constexpr uint16_t DEFAULT_GAME_PORT = 46860;
    constexpr int HOSTNAME_BUFFER_SIZE = 256;
    constexpr int RECV_BUFFER_SIZE = 1024;
    constexpr int LISTEN_BACKLOG = 1;
    constexpr const char* DEFAULT_LOOPBACK_IP = "127.0.0.1";
}

std::string GetLocalIPAddress();

class NetworkManager
{
public:
    enum class Mode
    {
        None,
        Host,
        Client
    };

    NetworkManager();
    virtual ~NetworkManager();

    bool Initialize();
    void Shutdown();
    void ForceDisconnect();
    bool StartHost(uint16_t port = NetworkConstants::DEFAULT_GAME_PORT);
    bool ConnectToHost(const char* ipAddress, uint16_t port = NetworkConstants::DEFAULT_GAME_PORT);
    void Update();
    bool SendPacket(const void* packetData, uint16_t size);
    bool PopPacket(std::vector<uint8_t>& outPacket);

    void SetOnDiceResult(std::function<void(int)> callback)
    {
        m_onDiceResult = callback;
    }
    void SetOnMovePiece(std::function<void(int, int, int, int)> callback)
    {
        m_onMovePiece = callback;
    }

    void SendDiceResult(int steps);
    void SendMovePiece(int fromX, int fromY, int toX, int toY);
    virtual void DispatchPackets();

    bool IsConnected() const
    {
        return m_isConnected;
    }
    Mode GetMode() const
    {
        return m_mode;
    }

    uint8_t GetAssignedPlayerType() const
    {
        return m_assignedPlayerType;
    }
    void SetAssignedPlayerType(uint8_t playerType)
    {
        m_assignedPlayerType = playerType;
    }

protected:
    uint8_t m_assignedPlayerType = 0;

private:
    void SetNonBlocking(SOCKET sock);

private:
    SOCKET m_listenSocket = INVALID_SOCKET;
    SOCKET m_peerSocket = INVALID_SOCKET;
    Mode m_mode = Mode::None;
    bool m_isConnected = false;

    std::vector<uint8_t> m_recvBuffer;
    std::vector<std::vector<uint8_t>> m_incomingPackets;

    std::function<void(int)> m_onDiceResult;
    std::function<void(int, int, int, int)> m_onMovePiece;
};