#pragma once

#include "pch.h"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "NetworkPackets.h"

// 自PCのローカルIPv4アドレス（192.168.x.x 等）を取得する関数
std::string GetLocalIPAddress();

// ============================================================================
// TCP/IPソケット通信の接続・切断・送受信を管理するネットワーク基底クラス
// ============================================================================
class NetworkManager
{
public:
    // 接続モード（未接続 / ホスト / クライアント）
    enum class Mode
    {
        None,
        Host,
        Client
    };

    NetworkManager();
    virtual ~NetworkManager();

    // Winsock の初期化処理
    bool Initialize();
    // 通信の終了処理
    void Shutdown();

    // SO_LINGER (linger=0) を使って即座に接続をリセット切断しソケットを破棄します
    void ForceDisconnect();

    // ホスト（部屋主）としてポートを開放し接続待機を開始
    bool StartHost(uint16_t port = 46860);
    // クライアントとして指定IPアドレスのホストへ接続
    bool ConnectToHost(const char* ipAddress, uint16_t port = 46860);
    // 毎フレームの受信ソケット監視およびバッファ展開
    void Update();

    // データの送信処理
    bool SendPacket(const void* packetData, uint16_t size);
    // 受信キューからパケットを1つ取り出す
    bool PopPacket(std::vector<uint8_t>& outPacket);

    // 各種イベントコールバックの設定
    void SetOnDiceResult(std::function<void(int)> callback)
    {
        m_onDiceResult = callback;
    }
    void SetOnMovePiece(std::function<void(int, int, int, int)> callback)
    {
        m_onMovePiece = callback;
    }

    // 簡易ヘルパー送信関数
    void SendDiceResult(int steps);
    void SendMovePiece(int fromX, int fromY, int toX, int toY);
    // 受信キューに溜まったパケットを解釈して登録済みコールバックを実行
    void DispatchPackets();

    // 接続状態の取得
    bool IsConnected() const
    {
        return m_isConnected;
    }
    // 現在の接続モード（Host / Client）を取得
    Mode GetMode() const
    {
        return m_mode;
    }

private:
    // ソケットを非ブロッキング（非同期）モードへ設定
    void SetNonBlocking(SOCKET sock);

private:
    SOCKET m_listenSocket = INVALID_SOCKET; // ホストの接続受付用ソケット
    SOCKET m_peerSocket = INVALID_SOCKET;   // 通信相手との接続用ソケット
    Mode m_mode = Mode::None;               // 接続モード
    bool m_isConnected = false;             // 接続中フラグ

    std::vector<uint8_t> m_recvBuffer;                   // 通信データ受信一次バッファ
    std::vector<std::vector<uint8_t>> m_incomingPackets; // 解析完了済みパケットキュー

    std::function<void(int)> m_onDiceResult;               // 出目受信時コールバック
    std::function<void(int, int, int, int)> m_onMovePiece; // 移動受信時コールバック
};