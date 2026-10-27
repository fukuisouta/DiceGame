#pragma once

#include <string>
#include <vector>
#include "Network/NetworkManager.h"

// ============================================================================
// デバッグ・テスト用の簡易ネットワーク設定UIクラス
// ============================================================================
class NetworkUI
{
public:
    NetworkUI() = default;
    ~NetworkUI() = default;

    // 初期化関数：ローカルIPアドレスを取得
    void Initialize();

    // ImGui によるテストUI描画
    void RenderUI(NetworkManager& networkManager);

    // 表示フラグの取得および設定
    bool IsVisible() const
    {
        return m_isVisible;
    }
    void SetVisible(bool visible)
    {
        m_isVisible = visible;
    }

private:
    bool m_isVisible = true;           // UI表示状態
    char m_ipBuffer[64] = "127.0.0.1"; // IPアドレスバッファ
    std::string m_statusMsg = "Ready"; // 状態表示テキスト
};