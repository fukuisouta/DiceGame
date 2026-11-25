#pragma once

#include <cstdint>
#include <string>
#include "ServerGameLogic.h"

// 部屋の状態列挙型
enum class RoomState
{
    WaitingForPlayers, // 参加者待機中 (1人のみ入室中)
    Playing,           // 対戦中 (2人着席完了)
    Finished           // 対局終了
};

// 1対1の対戦セッションを管理するルームクラス
class GameRoom
{
public:
    explicit GameRoom(uint32_t roomId) : m_roomId(roomId) {}
    ~GameRoom() = default;

    bool AddPlayer(uint32_t clientId);
    void RemovePlayer(uint32_t clientId);

    ServerPlayerType GetPlayerType(uint32_t clientId) const;
    uint32_t GetOpponentId(uint32_t clientId) const;

    // ルーム設定（モード・公開区分・3桁パスワード・ホスト名）
    void ConfigureRoom(uint8_t gameMode, bool isPrivate, const std::string& password, const std::string& hostName)
    {
        m_gameMode = gameMode;
        m_isPrivate = isPrivate;
        m_password = password;
        m_hostName = hostName.empty() ? "Host" : hostName;
    }

    void SetGuestName(const std::string& guestName)
    {
        m_guestName = guestName.empty() ? "Guest" : guestName;
    }

    uint8_t GetGameMode() const { return m_gameMode; }
    bool IsPrivate() const { return m_isPrivate; }
    const std::string& GetPassword() const { return m_password; }
    const std::string& GetHostName() const { return m_hostName; }
    const std::string& GetGuestName() const { return m_guestName; }

    // 先攻決めダイスの出目を記録・リセットする関数
    void SetFirstTurnRoll(uint32_t clientId, int32_t value);
    void ResetFirstTurnRolls() { m_p1FirstRoll = 0; m_p2FirstRoll = 0; }
    int32_t GetP1FirstRoll() const { return m_p1FirstRoll; }
    int32_t GetP2FirstRoll() const { return m_p2FirstRoll; }
    bool AreBothFirstRollsReady() const { return (m_p1FirstRoll > 0 && m_p2FirstRoll > 0); }

    uint32_t GetRoomId() const { return m_roomId; }
    RoomState GetState() const { return m_state; }
    uint32_t GetPlayer1Id() const { return m_player1Id; }
    uint32_t GetPlayer2Id() const { return m_player2Id; }
    ServerGameLogic& GetGameLogic() { return m_gameLogic; }

private:
    uint32_t m_roomId = 0;
    uint32_t m_player1Id = 0;
    uint32_t m_player2Id = 0;

    uint8_t m_gameMode = 0;          // 0: Original, 1: Advance
    bool m_isPrivate = false;        // true: 3桁パスワード付き, false: フリー
    std::string m_password = "";     // 3桁パスワード
    std::string m_hostName = "Host";
    std::string m_guestName = "Guest";

    int32_t m_p1FirstRoll = 0;
    int32_t m_p2FirstRoll = 0;
    RoomState m_state = RoomState::WaitingForPlayers;
    ServerGameLogic m_gameLogic;
};