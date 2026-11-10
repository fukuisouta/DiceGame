#pragma once
#include <string>

struct GameContext;

// オンラインロビーUI用定数定義
namespace LobbyUIConstants
{
    constexpr size_t BUFFER_SIZE = 64;       // 入力バッファサイズ
    constexpr int MODE_HOST_INDEX = 0;       // モード選択: ホスト(部屋作成)のラジオインデックス
    constexpr int MODE_CLIENT_INDEX = 1;     // モード選択: クライアント(参加)のラジオインデックス
    constexpr uint16_t DEFAULT_PORT = 46860; // ロビー接続ポート番号
}

// オンライン対戦ロビー画面のUI描画およびユーザー入力を制御するクラス
class NetworkLobbyUI
{
public:
    NetworkLobbyUI() = default;
    ~NetworkLobbyUI() = default;

    // 起動時にローカルIPを自動取得して表示バッファにセットする関数
    void Initialize();

    // オンライン対戦ロビーUIの全画面描画を行う関数
    // 引数 gameContext: ゲーム全体コンテキスト
    void Render(GameContext& gameContext);

    // 設定されたプレイヤーネーム文字列を取得する関数
    const std::string& GetPlayerName() const
    {
        return m_playerNameStr;
    }

    // UI表示の有効/非有効を切り替える関数
    void SetVisible(bool visible)
    {
        m_isVisible = visible;
    }

    // 現在UIが表示中かどうかを取得する関数
    bool IsVisible() const
    {
        return m_isVisible;
    }

private:
    bool m_isVisible = true; // UIを描画するかどうかのフラグ

    int m_selectedModeIndex = LobbyUIConstants::MODE_HOST_INDEX; // 選択中対戦モードインデックス (0: Host, 1: Client)

    char m_playerNameBuffer[LobbyUIConstants::BUFFER_SIZE] = "Player"; // プレイヤー名入力用バッファ
    std::string m_playerNameStr = "Player";                            // 確定したプレイヤー表示名

    std::string m_myIPAddress = "127.0.0.1";                            // 自PCの自動取得IPv4アドレス文字列
    char m_targetIpBuffer[LobbyUIConstants::BUFFER_SIZE] = "127.0.0.1"; // 接続相手のIP入力バッファ

    std::string m_statusMsg = "Ready"; // ロビー画面下部に表示する接続ステータスメッセージ
};