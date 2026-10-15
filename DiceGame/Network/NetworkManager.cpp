#include "pch.h"
#include "NetworkManager.h"
#include <iostream>

NetworkManager::NetworkManager()
{
}

NetworkManager::~NetworkManager()
{
    Shutdown();
}

bool NetworkManager::Initialize()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return (result == 0);
}

void NetworkManager::Shutdown()
{
    if (m_peerSocket != INVALID_SOCKET)
    {
        closesocket(m_peerSocket);
        m_peerSocket = INVALID_SOCKET;
    }
    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    WSACleanup();
    m_isConnected = false;
    m_mode = Mode::None;
}

void NetworkManager::SetNonBlocking(SOCKET sock)
{
    u_long mode = 1; // 1: ノンブロッキングモード
    ioctlsocket(sock, FIONBIO, &mode);
}

bool NetworkManager::StartHost(uint16_t port)
{
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
        return false;

    SetNonBlocking(m_listenSocket);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        return false;
    }

    m_mode = Mode::Host;
    return true;
}

bool NetworkManager::ConnectToHost(const char* ipAddress, uint16_t port)
{
    m_peerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_peerSocket == INVALID_SOCKET)
        return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    // 接続実行（ノンブロッキング前に行うか、WSAEWOULDBLOCK をハンドリング）
    int result = connect(m_peerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
        {
            closesocket(m_peerSocket);
            m_peerSocket = INVALID_SOCKET;
            return false;
        }
    }

    SetNonBlocking(m_peerSocket);
    m_mode = Mode::Client;
    m_isConnected = true; // 簡略化（必要に応じて毎フレーム接続状態を監視）
    return true;
}

void NetworkManager::Update()
{
    // 1. ホスト待機中の場合: 接続をチェック
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

    // 2. 接続中の場合: データを受信
    if (m_isConnected && m_peerSocket != INVALID_SOCKET)
    {
        char tempBuffer[1024];
        int bytesRead = recv(m_peerSocket, tempBuffer, sizeof(tempBuffer), 0);

        if (bytesRead > 0)
        {
            // 受信データをバッファに追加
            m_recvBuffer.insert(m_recvBuffer.end(), tempBuffer, tempBuffer + bytesRead);

            // パケット解析（ヘッダーの size に基づいて分割）
            while (m_recvBuffer.size() >= sizeof(PacketHeader))
            {
                PacketHeader* header = reinterpret_cast<PacketHeader*>(m_recvBuffer.data());
                if (m_recvBuffer.size() >= header->size)
                {
                    // 1パケット分切り出し
                    std::vector<uint8_t> packet(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
                    m_incomingPackets.push_back(packet);

                    // バッファの先頭から削除
                    m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
                }
                else
                {
                    break; // パケットがまだ全部届いていない
                }
            }
        }
        else if (bytesRead == 0)
        {
            // 切断された
            m_isConnected = false;
        }
        else
        {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
            {
                // エラーによる切断
                m_isConnected = false;
            }
        }
    }
}

bool NetworkManager::SendPacket(const void* packetData, uint16_t size)
{
    if (!m_isConnected || m_peerSocket == INVALID_SOCKET)
        return false;

    int result = send(m_peerSocket, reinterpret_cast<const char*>(packetData), size, 0);
    return (result != SOCKET_ERROR);
}

bool NetworkManager::PopPacket(std::vector<uint8_t>& outPacket)
{
    if (m_incomingPackets.empty())
        return false;

    outPacket = m_incomingPackets.front();
    m_incomingPackets.erase(m_incomingPackets.begin());
    return true;
}