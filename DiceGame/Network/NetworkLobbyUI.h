#pragma once
#include <string>

struct GameContext;

// ============================================================================
// オンライン対戦ロビー画面のUI描画およびユーザー入力を制御するクラス
// ImGui を使用してプレイヤー名入力や部屋作成・参加操作を提供します
// ============================================================================
class NetworkLobbyUI
{
public:
    NetworkLobbyUI() = default;
    ~NetworkLobbyUI() = default;

    // 起動時にローカルIPを自動取得して表示用バッファにセット
    void Initialize();

    // UIの描画処理（毎フレーム呼び出し）
    void Render(GameContext& gameContext);

    // 設定されたプレイヤーネームの取得
    const std::string& GetPlayerName() const
    {
        return m_playerNameStr;
    }

    // UIの表示/非表示フラグの切り替え
    void SetVisible(bool visible)
    {
        m_isVisible = visible;
    }
    bool IsVisible() const
    {
        return m_isVisible;
    }

private:
    bool m_isVisible = true; // UI描画フラグ

    int m_selectedModeIndex = 0; // 接続モード選択（0: Host, 1: Client）

    char m_playerNameBuffer[64] = "Player"; // プレイヤー名入力用バッファ
    std::string m_playerNameStr = "Player"; // 確定プレイヤー名文字列

    std::string m_myIPAddress = "127.0.0.1"; // 自PCのIPv4アドレス（自動取得結果）
    char m_targetIpBuffer[64] = "127.0.0.1"; // 接続先ホストのIPアドレス入力用バッファ

    std::string m_statusMsg = "Ready"; // 画面下部に表示するステータスメッセージ
};