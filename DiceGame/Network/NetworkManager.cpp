#include "pch.h"
#include "NetworkManager.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

// 自PCのローカルIPv4アドレスを取得
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

// コンストラクタ
NetworkManager::NetworkManager()
{
}

// デストラクタ：強制リセット切断の呼び出し
NetworkManager::~NetworkManager()
{
    ForceDisconnect();
}

// Winsockライブラリの初期化
bool NetworkManager::Initialize()
{
    WSADATA wsaData;
    constexpr WORD wsaVersion = MAKEWORD(2, 2); // Winsock 2.2 バージョン指定
    int result = WSAStartup(wsaVersion, &wsaData);
    return (result == 0);
}

// 即座にRSTフラグを送信してソケットを破棄する強硬切断処理
void NetworkManager::ForceDisconnect()
{
    auto CloseSocketWithLinger = [](SOCKET& sock)
    {
        if (sock != INVALID_SOCKET)
        {
            // linger 構造体の設定 (l_onoff = 1, l_linger = 0 で強制リセット)
            constexpr u_short LINGER_ENABLE = 1;
            constexpr u_short LINGER_TIME_ZERO = 0;

            linger ling;
            ling.l_onoff = LINGER_ENABLE;
            ling.l_linger = LINGER_TIME_ZERO;
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

    WSACleanup();
}

// シャットダウン処理
void NetworkManager::Shutdown()
{
    ForceDisconnect();
}

// ソケットの非ブロッキング化設定
void NetworkManager::SetNonBlocking(SOCKET sock)
{
    constexpr u_long NON_BLOCKING_MODE = 1; // 非ブロッキングモード有効化フラグ
    u_long mode = NON_BLOCKING_MODE;
    ioctlsocket(sock, FIONBIO, &mode);
}

// ホストとしての接続受付開始
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

    if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(m_listenSocket, NetworkConstants::LISTEN_BACKLOG) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    m_mode = Mode::Host;
    m_isConnected = false;
    return true;
}

// クライアントとしてのホスト接続要求
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

// 毎フレームの受信ソケット監視およびパケット解析
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
        int bytesRecv = recv(m_peerSocket, buffer, sizeof(buffer), 0);
        if (bytesRecv > 0)
        {
            m_recvBuffer.insert(m_recvBuffer.end(), buffer, buffer + bytesRecv);

            while (m_recvBuffer.size() >= sizeof(PacketHeader))
            {
                PacketHeader* header = reinterpret_cast<PacketHeader*>(m_recvBuffer.data());
                if (m_recvBuffer.size() < header->size)
                    break;

                std::vector<uint8_t> packet(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
                m_incomingPackets.push_back(packet);
                m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
            }
        }
        else if (bytesRecv == 0 || (bytesRecv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
        {
            m_isConnected = false;
        }
    }
}

// データ送信実行
bool NetworkManager::SendPacket(const void* packetData, uint16_t size)
{
    if (!m_isConnected || m_peerSocket == INVALID_SOCKET)
        return false;

    int bytesSent = send(m_peerSocket, reinterpret_cast<const char*>(packetData), size, 0);
    return (bytesSent != SOCKET_ERROR);
}

// 受信キューからの取り出し
bool NetworkManager::PopPacket(std::vector<uint8_t>& outPacket)
{
    if (m_incomingPackets.empty())
        return false;

    outPacket = m_incomingPackets.front();
    m_incomingPackets.erase(m_incomingPackets.begin());
    return true;
}

// ダイス出目送信
void NetworkManager::SendDiceResult(int steps)
{
    MsgDiceResult packet;
    packet.header.size = sizeof(MsgDiceResult);
    packet.header.type = PacketType::CS_DiceResult;
    packet.steps = steps;

    SendPacket(&packet, sizeof(packet));
}

// コマ移動送信
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

// パケットディスパッチとイベントハンドラ呼び出し
void NetworkManager::DispatchPackets()
{
    Update();

    std::vector<uint8_t> packetData;
    while (PopPacket(packetData))
    {
        if (packetData.size() < sizeof(PacketHeader))
            continue;

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packetData.data());

        if (header->type == PacketType::CS_DiceResult)
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