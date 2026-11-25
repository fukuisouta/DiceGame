#include "pch.h"
#include "NetworkManager.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

std::string GetLocalIPAddress()
{
    char hostName[NetworkConstants::HOSTNAME_BUFFER_SIZE] = {0};
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR)
    {
        return NetworkConstants::DEFAULT_LOOPBACK_IP;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &res) != 0 || !res)
    {
        return NetworkConstants::DEFAULT_LOOPBACK_IP;
    }

    char ipStr[INET_ADDRSTRLEN] = {0};
    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipStr, sizeof(ipStr));

    freeaddrinfo(res);
    return std::string(ipStr);
}

NetworkManager::NetworkManager()
{
}
NetworkManager::~NetworkManager()
{
    ForceDisconnect();
}

bool NetworkManager::Initialize()
{
    WSADATA wsaData;
    constexpr WORD wsaVersion = MAKEWORD(2, 2);
    int result = WSAStartup(wsaVersion, &wsaData);
    return (result == 0);
}

void NetworkManager::ForceDisconnect()
{
    auto CloseSocketWithLinger = [](SOCKET& sock)
    {
        if (sock != INVALID_SOCKET)
        {
            linger ling{1, 0};
            setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&ling), sizeof(ling));
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    };

    CloseSocketWithLinger(m_peerSocket);
    CloseSocketWithLinger(m_listenSocket);

    m_recvBuffer.clear();
    m_incomingPackets.clear();
    m_isConnected = false;
    m_mode = Mode::None;
    m_assignedPlayerType = 0;

    WSACleanup();
}

void NetworkManager::Shutdown()
{
    ForceDisconnect();
}

void NetworkManager::SetNonBlocking(SOCKET sock)
{
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    // Nagleアルゴリズムを無効化してリアルタイム通信の遅延・引っかかりを解消
    BOOL noDelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
}

bool NetworkManager::StartHost(uint16_t port)
{
    ForceDisconnect();
    Initialize();

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
        return false;

    SetNonBlocking(m_listenSocket);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(m_listenSocket, NetworkConstants::LISTEN_BACKLOG) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    m_mode = Mode::Host;
    m_isConnected = false;
    return true;
}

bool NetworkManager::ConnectToHost(const char* ipAddress, uint16_t port)
{
    ForceDisconnect();
    Initialize();

    m_peerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_peerSocket == INVALID_SOCKET)
        return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress, &addr.sin_addr);

    if (connect(m_peerSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_peerSocket);
        m_peerSocket = INVALID_SOCKET;
        return false;
    }

    SetNonBlocking(m_peerSocket);
    m_mode = Mode::Client;
    m_isConnected = true;
    return true;
}

void NetworkManager::Update()
{
    if (m_mode == Mode::Host && !m_isConnected)
    {
        SOCKET clientSock = accept(m_listenSocket, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET)
        {
            m_peerSocket = clientSock;
            SetNonBlocking(m_peerSocket);
            m_isConnected = true;
        }
    }

    if (m_isConnected && m_peerSocket != INVALID_SOCKET)
    {
        char buffer[NetworkConstants::RECV_BUFFER_SIZE];

        // ソケットに溜まっている受信データを空になるまで一気に読み切る
        while (true)
        {
            int bytesRecv = recv(m_peerSocket, buffer, sizeof(buffer), 0);
            if (bytesRecv > 0)
            {
                m_recvBuffer.insert(m_recvBuffer.end(), buffer, buffer + bytesRecv);
            }
            else if (bytesRecv == 0)
            {
                m_isConnected = false;
                m_assignedPlayerType = 0;
                break;
            }
            else
            {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    m_isConnected = false;
                    m_assignedPlayerType = 0;
                }
                break;
            }
        }

        // 受信バッファから完全なパケットを切り出す
        while (m_recvBuffer.size() >= sizeof(PacketHeader))
        {
            PacketHeader* header = reinterpret_cast<PacketHeader*>(m_recvBuffer.data());
            if (header->size < sizeof(PacketHeader) || m_recvBuffer.size() < header->size)
                break;

            std::vector<uint8_t> packet(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
            m_incomingPackets.push_back(packet);
            m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
        }
    }
}

bool NetworkManager::SendPacket(const void* packetData, uint16_t size)
{
    if (!m_isConnected || m_peerSocket == INVALID_SOCKET)
        return false;
    int bytesSent = send(m_peerSocket, reinterpret_cast<const char*>(packetData), size, 0);
    return (bytesSent != SOCKET_ERROR);
}

bool NetworkManager::PopPacket(std::vector<uint8_t>& outPacket)
{
    if (m_incomingPackets.empty())
        return false;
    outPacket = m_incomingPackets.front();
    m_incomingPackets.erase(m_incomingPackets.begin());
    return true;
}

void NetworkManager::SendDiceResult(int steps)
{
    MsgDiceResult packet;
    packet.header.size = sizeof(MsgDiceResult);
    packet.header.type = PacketType::SC_DiceResult;
    packet.steps = steps;
    SendPacket(&packet, sizeof(packet));
}

void NetworkManager::SendMovePiece(int fromX, int fromY, int toX, int toY)
{
    MsgMovePiece packet;
    packet.header.size = sizeof(MsgMovePiece);
    packet.header.type = PacketType::CS_MovePiece;
    packet.fromX = fromX;
    packet.fromY = fromY;
    packet.toX = toX;
    packet.toY = toY;
    SendPacket(&packet, sizeof(packet));
}

void NetworkManager::DispatchPackets()
{
    Update();

    std::vector<uint8_t> packetData;
    while (PopPacket(packetData))
    {
        if (packetData.size() < sizeof(PacketHeader))
            continue;

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packetData.data());

        if (header->type == PacketType::SC_MatchStart && packetData.size() >= sizeof(MsgMatchStart))
        {
            const auto* pkt = reinterpret_cast<const MsgMatchStart*>(packetData.data());
            m_assignedPlayerType = pkt->assignedPlayerType;
        }
        else if (header->type == PacketType::SC_DiceResult)
        {
            const auto* pkt = reinterpret_cast<const MsgDiceResult*>(packetData.data());
            if (m_onDiceResult)
                m_onDiceResult(pkt->steps);
        }
        else if (header->type == PacketType::CS_MovePiece)
        {
            const auto* pkt = reinterpret_cast<const MsgMovePiece*>(packetData.data());
            if (m_onMovePiece)
                m_onMovePiece(pkt->fromX, pkt->fromY, pkt->toX, pkt->toY);
        }
    }
}