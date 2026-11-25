#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCK_HANDLE = INVALID_SOCKET;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCK_HANDLE = -1;
#endif

#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <cstdint>
#include <functional>

struct ClientSession
{
    uint32_t id = 0;
    SocketHandle socket = INVALID_SOCK_HANDLE;
    sockaddr_in address{};
    std::vector<uint8_t> recvBuffer;
};

struct PacketMessage
{
    uint32_t clientId = 0;
    uint16_t packetType = 0;
    std::vector<uint8_t> body;
};

class ServerNetworkManager
{
public:
    ServerNetworkManager() = default;
    ~ServerNetworkManager();

    bool Start(uint16_t port);
    void Stop();
    void Update();

    bool SendPacket(uint32_t clientId, const void* data, size_t size);
    void BroadcastPacket(const void* data, size_t size);
    bool PopPacket(PacketMessage& outPacket);

    void SetOnConnectCallback(std::function<void(uint32_t)> cb) { m_onConnect = cb; }
    void SetOnDisconnectCallback(std::function<void(uint32_t)> cb) { m_onDisconnect = cb; }

private:
    void SetNonBlocking(SocketHandle sock);
    void CloseSocketHandle(SocketHandle sock);
    void ProcessRecvBuffer(ClientSession& session);

private:
    SocketHandle m_listenSocket = INVALID_SOCK_HANDLE;
    bool m_isRunning = false;
    uint32_t m_nextClientId = 1;
    std::unordered_map<uint32_t, ClientSession> m_clients;

    std::queue<PacketMessage> m_packetQueue;
    std::mutex m_queueMutex;

    std::function<void(uint32_t)> m_onConnect;
    std::function<void(uint32_t)> m_onDisconnect;
};