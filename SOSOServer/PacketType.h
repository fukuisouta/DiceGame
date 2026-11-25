#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
    CS_PlayerInfo = 1,
    SC_MatchStart = 2,
    CS_DiceRoll = 3,
    SC_DiceResult = 4,
    CS_MovePiece = 5,
    SC_MovePiece = 6,
    CS_SkillUse = 7,
    SC_SkillUse = 8,
    CS_EndTurn = 9,
    SC_TurnChange = 10,
    SC_WorldSync = 11,
    CS_FirstTurnRoll = 12,
    SC_FirstTurnResult = 13,

    // --- ロビー・ルーム管理用パケット ---
    CS_CreateRoom = 20,      // ルーム作成要求 (モード・公開区分・3桁PW・ホスト名)
    SC_RoomCreated = 21,     // ルーム作成完了通知 (RoomID)
    CS_RequestRoomList = 22, // 募集中ルーム一覧要求
    SC_RoomList = 23,        // 募集中ルーム一覧応答
    CS_JoinRoom = 24,        // ルーム参加要求 (RoomID・3桁PW・参加者名)
    SC_JoinRoomResult = 25,  // ルーム参加結果 (成功/PW不一致/満員)
    CS_CancelRoom = 26       // ホストによるルーム待機キャンセル
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size;
    uint16_t packetType;
};

struct MsgMatchStart
{
    PacketHeader header;
    uint8_t assignedPlayerType; // 1 = Player1(Host), 2 = Player2(Guest)
    uint8_t gameMode;           // 0 = Original, 1 = Advance
    char opponentName[32];      // 対戦相手のプレイヤー名
};

struct MsgFirstTurnRoll
{
    PacketHeader header;
    int32_t rollValue;
};

struct MsgFirstTurnResult
{
    PacketHeader header;
    int32_t p1Roll;
    int32_t p2Roll;
    uint8_t firstPlayer;
};

struct MsgCreateRoom
{
    PacketHeader header;
    uint8_t gameMode;     // 0 = Original, 1 = Advance
    uint8_t isPrivate;    // 0 = フリー, 1 = プライベート(3桁PW)
    char password[4];     // 数字3桁 + '\0'
    char hostName[32];    // ホストのプレイヤー名
};

struct MsgRoomCreated
{
    PacketHeader header;
    uint32_t roomId;
};

struct RoomSummaryInfo
{
    uint32_t roomId;
    uint8_t gameMode;     // 0 = Original, 1 = Advance
    uint8_t isPrivate;    // 0 = フリー, 1 = プライベート
    char hostName[32];
};

constexpr size_t MAX_LISTED_ROOMS = 16;

struct MsgRoomList
{
    PacketHeader header;
    uint8_t roomCount;
    RoomSummaryInfo rooms[MAX_LISTED_ROOMS];
};

struct MsgJoinRoom
{
    PacketHeader header;
    uint32_t roomId;
    char password[4];     // 数字3桁 + '\0'
    char guestName[32];   // 参加者のプレイヤー名
};

struct MsgJoinRoomResult
{
    PacketHeader header;
    uint8_t success;      // 1 = 成功, 0 = 失敗
    uint8_t reason;       // 0 = OK, 1 = ルームが存在しない/満員, 2 = パスワード不一致
};
#pragma pack(pop)