#include "pch.h"
#include "NetworkManager.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

// 自PCのローカルIPアドレスを取得して文字列形式で返します
std::string GetLocalIPAddress()
{
    char hostName[256] = {0};
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR)
    {
        return "127.0.0.1";
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &res) != 0 || !res)
    {
        return "127.0.0.1";
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
    ForceDisconnect(); // インスタンス破棄時は強制切断を実行
}

// Winsock 2.2 ライブラリの初期化
bool NetworkManager::Initialize()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return (result == 0);
}

// 送信バッファを破棄してRST（即時リセット）を送信する強制切断処理
void NetworkManager::ForceDisconnect()
{
    auto CloseSocketWithLinger = [](SOCKET& sock)
    {
        if (sock != INVALID_SOCKET)
        {
            // linger 構造体の設定 (l_onoff = 1, l_linger = 0 で即時強制リセット)
            linger ling;
            ling.l_onoff = 1;
            ling.l_linger = 0;
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

void NetworkManager::Shutdown()
{
    ForceDisconnect();
}

// ソケットを非ブロッキングモード（recv/connectでスレッドが停止しない設定）へ切り替え
void NetworkManager::SetNonBlocking(SOCKET sock)
{
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
}

// 部屋主（ホスト）としてポートをバインドしクライアントの接続受付を開始
bool NetworkManager::StartHost(uint16_t port)
{
    ForceDisconnect(); // 再起動前に過去のソケットをリセット
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

    if (listen(m_listenSocket, 1) == SOCKET_ERROR)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    m_mode = Mode::Host;
    m_isConnected = false;
    return true;
}

// クライアントとして指定されたホストのIP・ポートへ接続要求を送信
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

// 毎フレーム呼び出し：接続確立の監視およびパケットの受信ストリーム展開
void NetworkManager::Update()
{
    // ホストモードで未接続の場合、新規クライアントの接続をチェック
    if (m_mode == Mode::Host && !m_isConnected)
    {
        SOCKET clientSock = accept(m_listenSocket, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET)
        {
            m_peerSocket = clientSock;
            SetNonBlocking(m_peerSocket);
            m_isConnected = true; // クライアント接続完了
        }
    }

    // 接続相手から送られてくるデータの受信・パケット切り出し処理
    if (m_isConnected && m_peerSocket != INVALID_SOCKET)
    {
        char buffer[1024];
        int bytesRecv = recv(m_peerSocket, buffer, sizeof(buffer), 0);
        if (bytesRecv > 0)
        {
            // 受信バッファへ追加
            m_recvBuffer.insert(m_recvBuffer.end(), buffer, buffer + bytesRecv);

            // ヘッダーサイズ以上のデータが蓄積されている場合、パケット単位へ分割切り出し
            while (m_recvBuffer.size() >= sizeof(PacketHeader))
            {
                PacketHeader* header = reinterpret_cast<PacketHeader*>(m_recvBuffer.data());
                if (m_recvBuffer.size() < header->size)
                    break; // パケット全体の到着を待つ

                // 完全なパケットデータを抽出してキューに追加
                std::vector<uint8_t> packet(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
                m_incomingPackets.push_back(packet);
                m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + header->size);
            }
        }
        else if (bytesRecv == 0 || (bytesRecv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
        {
            // 切断通知またはエラー検出時の切断処理
            m_isConnected = false;
        }
    }
}

// パケットデータの非同期送信
bool NetworkManager::SendPacket(const void* packetData, uint16_t size)
{
    if (!m_isConnected || m_peerSocket == INVALID_SOCKET)
        return false;

    int bytesSent = send(m_peerSocket, reinterpret_cast<const char*>(packetData), size, 0);
    return (bytesSent != SOCKET_ERROR);
}

// 受信パケットキューから先頭パケットを取り出す
bool NetworkManager::PopPacket(std::vector<uint8_t>& outPacket)
{
    if (m_incomingPackets.empty())
        return false;

    outPacket = m_incomingPackets.front();
    m_incomingPackets.erase(m_incomingPackets.begin());
    return true;
}

// ダイス出目の同期送信
void NetworkManager::SendDiceResult(int steps)
{
    MsgDiceResult packet;
    packet.header.size = sizeof(MsgDiceResult);
    packet.header.type = PacketType::CS_DiceResult;
    packet.steps = steps;

    SendPacket(&packet, sizeof(packet));
}

// コマ移動の同期送信
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

// 受信パケットのディスパッチ（コールバックの自動実行）
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