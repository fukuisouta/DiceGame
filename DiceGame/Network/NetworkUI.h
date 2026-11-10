#pragma once

#include <string>
#include <vector>
#include "Network/NetworkManager.h"

// 簡易デバッグUI定数定義
namespace NetworkUIConstants
{
    constexpr size_t IP_BUFFER_SIZE = 64;  // IPアドレス入力バッファサイズ
    constexpr uint16_t DEBUG_PORT = 12345; // デバッグ接続テスト用ポート番号
}

// デバッグ・テスト用の簡易ネットワーク設定UIクラス
class NetworkUI
{
public:
    NetworkUI() = default;
    ~NetworkUI() = default;

    // 初期化：自身のローカルIPアドレスを取得する関数
    void Initialize();

    // ImGui によるテストUI描画を行う関数
    // 引数 networkManager: 通信マネージャーの参照
    void RenderUI(NetworkManager& networkManager);

    // デバッグUIの表示状態を取得する関数
    bool IsVisible() const
    {
        return m_isVisible;
    }

    // デバッグUIの表示状態を設定する関数
    void SetVisible(bool visible)
    {
        m_isVisible = visible;
    }

private:
    bool m_isVisible = true;                                           // UI表示中フラグ
    char m_ipBuffer[NetworkUIConstants::IP_BUFFER_SIZE] = "127.0.0.1"; // IPアドレス入力バッファ
    std::string m_statusMsg = "Ready";                                 // 状態表示テキスト
};