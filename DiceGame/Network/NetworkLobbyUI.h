#pragma once
#include <string>

struct GameContext;

class NetworkLobbyUI
{
public:
    NetworkLobbyUI() = default;
    ~NetworkLobbyUI() = default;

    // UIの描画と入力処理（シーンの Render から呼ぶ）
    void Render(GameContext& gameContext);

    // ウィンドウの表示/非表示切り替え
    void SetVisible(bool visible)
    {
        m_isVisible = visible;
    }
    bool IsVisible() const
    {
        return m_isVisible;
    }

private:
    bool m_isVisible = true;
    char m_ipBuffer[64] = "127.0.0.1";
    std::string m_statusMsg = "Ready";
};