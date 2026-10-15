
#pragma once
#include "pch.h"
#include <vector>
#include <cstdint>
#include "NetworkPackets.h"

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
    virtual ~NetworkManager(); // ★ 仮想デストラクタに変更

    bool Initialize();
    void Shutdown();

    bool StartHost(uint16_t port = 12345);
    bool ConnectToHost(const char* ipAddress, uint16_t port = 12345);
    void Update();

    bool SendPacket(const void* packetData, uint16_t size);
    bool PopPacket(std::vector<uint8_t>& outPacket);

    bool IsConnected() const
    {
        return m_isConnected;
    }
    Mode GetMode() const
    {
        return m_mode;
    }

private:
    void SetNonBlocking(SOCKET sock);

private:
    SOCKET m_listenSocket = INVALID_SOCKET;
    SOCKET m_peerSocket = INVALID_SOCKET;
    Mode m_mode = Mode::None;
    bool m_isConnected = false;

    std::vector<uint8_t> m_recvBuffer;
    std::vector<std::vector<uint8_t>> m_incomingPackets;
};