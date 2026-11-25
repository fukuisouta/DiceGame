#include "ServerNetworkManager.h"
#include <iostream>
#include <cstring>

#ifndef _WIN32
#include <csignal>
#endif

ServerNetworkManager::~ServerNetworkManager()
{
    Stop();
}

void ServerNetworkManager::SetNonBlocking(SocketHandle sock)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void ServerNetworkManager::CloseSocketHandle(SocketHandle sock)
{
    if (sock == INVALID_SOCK_HANDLE) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

bool ServerNetworkManager::Start(uint16_t port)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#else
    // クライアント切断時のSIGPIPEによるサーバー強制終了を防止
    std::signal(SIGPIPE, SIG_IGN);
#endif

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCK_HANDLE)
    {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // 再起動時の TIME_WAIT による bind 失敗を防止
    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    SetNonBlocking(m_listenSocket);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0 ||
        listen(m_listenSocket, SOMAXCONN) < 0)
    {
        CloseSocketHandle(m_listenSocket);
        m_listenSocket = INVALID_SOCK_HANDLE;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    m_isRunning = true;
    return true;
}

void ServerNetworkManager::Stop()
{
    if (!m_isRunning) return;
    m_isRunning = false;

    for (auto& [id, session] : m_clients)
    {
        CloseSocketHandle(session.socket);
    }
    m_clients.clear();

    CloseSocketHandle(m_listenSocket);
    m_listenSocket = INVALID_SOCK_HANDLE;

#ifdef _WIN32
    WSACleanup();
#endif
}

void ServerNetworkManager::Update()
{
    if (!m_isRunning) return;

    sockaddr_in clientAddr{};
#ifdef _WIN32
    int addrLen = sizeof(clientAddr);
#else
    socklen_t addrLen = sizeof(clientAddr);
#endif
    SocketHandle clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

    if (clientSocket != INVALID_SOCK_HANDLE)
    {
        SetNonBlocking(clientSocket);
        uint32_t newId = m_nextClientId++;
        m_clients[newId] = ClientSession{ newId, clientSocket, clientAddr,{} };

        if (m_onConnect) m_onConnect(newId);
    }

    std::vector<uint32_t> disconnectedIds;
    char tempBuffer[1024];

    for (auto& [id, session] : m_clients)
    {
        int bytesRecv = static_cast<int>(recv(session.socket, tempBuffer, sizeof(tempBuffer), 0));

        if (bytesRecv > 0)
        {
            session.recvBuffer.insert(session.recvBuffer.end(), tempBuffer, tempBuffer + bytesRecv);
            ProcessRecvBuffer(session);
        }
        else if (bytesRecv == 0)
        {
            disconnectedIds.push_back(id);
        }
        else
        {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                disconnectedIds.push_back(id);
            }
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                disconnectedIds.push_back(id);
            }
#endif
        }
    }

    for (uint32_t id : disconnectedIds)
    {
        auto it = m_clients.find(id);
        if (it != m_clients.end())
        {
            CloseSocketHandle(it->second.socket);
            m_clients.erase(it);
            if (m_onDisconnect) m_onDisconnect(id);
        }
    }
}

void ServerNetworkManager::ProcessRecvBuffer(ClientSession& session)
{
    constexpr size_t HEADER_SIZE = 4;

    while (session.recvBuffer.size() >= HEADER_SIZE)
    {
        uint16_t packetSize = 0;
        uint16_t packetType = 0;
        std::memcpy(&packetSize, &session.recvBuffer[0], sizeof(uint16_t));
        std::memcpy(&packetType, &session.recvBuffer[2], sizeof(uint16_t));

        if (packetSize < HEADER_SIZE || session.recvBuffer.size() < packetSize) break;

        PacketMessage msg;
        msg.clientId = session.id;
        msg.packetType = packetType;
        msg.body.assign(session.recvBuffer.begin(), session.recvBuffer.begin() + packetSize);

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(msg);
        }

        session.recvBuffer.erase(session.recvBuffer.begin(), session.recvBuffer.begin() + packetSize);
    }
}

bool ServerNetworkManager::SendPacket(uint32_t clientId, const void* data, size_t size)
{
    auto it = m_clients.find(clientId);
    if (it == m_clients.end() || it->second.socket == INVALID_SOCK_HANDLE) return false;
#ifdef _WIN32
    return (send(it->second.socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0) != SOCKET_ERROR);
#else
    return (send(it->second.socket, data, size, MSG_NOSIGNAL) >= 0);
#endif
}

void ServerNetworkManager::BroadcastPacket(const void* data, size_t size)
{
    for (const auto& [id, session] : m_clients)
    {
        if (session.socket != INVALID_SOCK_HANDLE)
        {
#ifdef _WIN32
            send(session.socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
#else
            send(session.socket, data, size, MSG_NOSIGNAL);
#endif
        }
    }
}

bool ServerNetworkManager::PopPacket(PacketMessage& outPacket)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_packetQueue.empty()) return false;
    outPacket = m_packetQueue.front();
    m_packetQueue.pop();
    return true;
}