#pragma once

#include "pch.h"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "NetworkPackets.h"

// ネットワークマネージャー用定数定義
namespace NetworkConstants
{
    constexpr uint16_t DEFAULT_GAME_PORT = 46860;            // デフォルトのゲーム通信ポート番号
    constexpr int HOSTNAME_BUFFER_SIZE = 256;                // ホスト名取得用バッファサイズ
    constexpr int RECV_BUFFER_SIZE = 1024;                   // 一時ソケット受信バッファサイズ
    constexpr int LISTEN_BACKLOG = 1;                        // 接続受付ソケットのキュー最大数
    constexpr const char* DEFAULT_LOOPBACK_IP = "127.0.0.1"; // ディフォルトループレベルIP
}

// 自PCのローカルIPv4アドレスを取得する関数
// 戻り値: IPv4アドレス文字列
std::string GetLocalIPAddress();

// TCP/IPソケット通信の接続・切断・送受信を管理するネットワーク基底クラス
class NetworkManager
{
public:
    // 接続モード列挙型
    enum class Mode
    {
        None,  // 未接続
        Host,  // ホスト（部屋主）
        Client // クライアント（参加者）
    };

    NetworkManager();
    virtual ~NetworkManager();

    // Winsock 2.2 ライブラリの初期化を実行する関数
    // 戻り値: 初期化成功時 true, 失敗時 false
    bool Initialize();

    // 通信の終了処理およびリソース開放を行う関数
    void Shutdown();

    // 即座にソケットを強制リセット切断し破棄する関数
    void ForceDisconnect();

    // ホスト（部屋主）としてポートを開放しクライアント接続待機を開始する関数
    // 引数 port: 待受ポート番号
    // 戻り値: 待機開始成功時 true, 失敗時 false
    bool StartHost(uint16_t port = NetworkConstants::DEFAULT_GAME_PORT);

    // クライアントとして指定されたホストのIP・ポートへ接続する関数
    // 引数 ipAddress: 接続先ホストのIPアドレス, port: ポート番号
    // 戻り値: 接続成功時 true, 失敗時 false
    bool ConnectToHost(const char* ipAddress, uint16_t port = NetworkConstants::DEFAULT_GAME_PORT);

    // 毎フレームの受信ソケット監視およびバッファ展開を行う関数
    void Update();

    // パケットデータを相手へ送信する関数
    // 引数 packetData: 送信データバッファのポインタ, size: データサイズ (バイト)
    // 戻り値: 送信成功時 true, 失敗時 false
    bool SendPacket(const void* packetData, uint16_t size);

    // 受信パケットキューから先頭パケットを1つ取り出す関数
    // 引数 outPacket: 取り出したデータを受け取るベクトル参照
    // 戻り値: パケットが存在し取得できた場合 true, キューが空の場合 false
    bool PopPacket(std::vector<uint8_t>& outPacket);

    // ダイス出目受信時のコールバック関数を登録する関数
    void SetOnDiceResult(std::function<void(int)> callback)
    {
        m_onDiceResult = callback;
    }

    // コマ移動受信時のコールバック関数を登録する関数
    void SetOnMovePiece(std::function<void(int, int, int, int)> callback)
    {
        m_onMovePiece = callback;
    }

    // ダイス出目同期パケットを相手へ送信するヘルパー関数
    // 引数 steps: 決定された出目（歩数）
    void SendDiceResult(int steps);

    // コマ移動同期パケットを相手へ送信するヘルパー関数
    // 引数 fromX, fromY: 移動元座標, toX, toY: 移動先座標
    void SendMovePiece(int fromX, int fromY, int toX, int toY);

    // 受信キューのパケットを解釈し、登録済みコールバックを実行する関数
    void DispatchPackets();

    // 現在通信相手と接続中かどうかを取得する関数
    bool IsConnected() const
    {
        return m_isConnected;
    }

    // 現在の接続モード（Host / Client / None）を取得する関数
    Mode GetMode() const
    {
        return m_mode;
    }

private:
    // ソケットを非ブロッキング（非同期）モードへ切り替える内部関数
    // 引数 sock: 対象のソケット記述子
    void SetNonBlocking(SOCKET sock);

private:
    SOCKET m_listenSocket = INVALID_SOCKET; // ホスト時の接続受付待機用ソケット記述子
    SOCKET m_peerSocket = INVALID_SOCKET;   // 通信相手との接続用ソケット記述子
    Mode m_mode = Mode::None;               // 現在の接続モード (None, Host, Client)
    bool m_isConnected = false;             // 通信相手との接続が確率しているかを示すフラグ

    std::vector<uint8_t> m_recvBuffer;                   // 受信済みのバイト列を一次蓄積するストリームバッファ
    std::vector<std::vector<uint8_t>> m_incomingPackets; // ヘッダー切り出しが完了した完全なパケットのキュー

    std::function<void(int)> m_onDiceResult;               // 出目受信時に起動されるイベントハンドラ
    std::function<void(int, int, int, int)> m_onMovePiece; // コマ移動受信時に起動されるイベントハンドラ
};