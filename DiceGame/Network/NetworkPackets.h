#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
    CS_PlayerInfo = 1,
    SC_MatchStart = 2,
    CS_DiceRoll = 3,
    SC_DiceResult = 4,
    CS_DiceResult = 4, // 互換用エイリアス
    CS_MovePiece = 5,
    SC_MovePiece = 6,
    CS_SkillUse = 7,
    SC_SkillUse = 8,
    CS_EndTurn = 9,
    SC_TurnChange = 10,
    SC_WorldSync = 11,
    CS_FirstTurnRoll = 12,
    SC_FirstTurnResult = 13,
    CS_SelectMode = 14,

    // ロビー・ルーム管理用パケット
    CS_CreateRoom = 20,
    SC_RoomCreated = 21,
    CS_RequestRoomList = 22,
    SC_RoomList = 23,
    CS_JoinRoom = 24,
    SC_JoinRoomResult = 25,
    CS_CancelRoom = 26
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size;
    PacketType type;
};

struct MsgPlayerInfo
{
    PacketHeader header;
    char playerName[32];
};

struct MsgMatchStart
{
    PacketHeader header;
    uint8_t assignedPlayerType; // 1 = Player1, 2 = Player2
    uint8_t gameMode;           // 0 = Original, 1 = Advance
    char opponentName[32];
};

struct MsgSelectMode
{
    PacketHeader header;
    uint8_t mode; // 0 = Original, 1 = Advance
};

struct MsgDiceResult
{
    PacketHeader header;
    int32_t steps;
};

struct MsgMovePiece
{
    PacketHeader header;
    int32_t fromX;
    int32_t fromY;
    int32_t toX;
    int32_t toY;
};

struct MsgSkillUse
{
    PacketHeader header;
    uint8_t skillType;
    int32_t targetX;
    int32_t targetY;
};

struct TransformData
{
    float posX, posY, posZ;
    float rotX, rotY, rotZ, rotW;
};

struct PieceSyncData
{
    uint8_t owner;
    uint8_t type;
    int8_t gridX;
    int8_t gridY;
    bool isActive;
    TransformData transform;
};

struct MsgWorldSync
{
    PacketHeader header;
    TransformData stepDiceTransform;
    uint8_t pieceCount;
    PieceSyncData pieces[10];
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
    uint8_t gameMode;  // 0 = Original, 1 = Advance
    uint8_t isPrivate; // 0 = フリー, 1 = プライベート(3桁PW)
    char password[4];  // 数字3桁 + '\0'
    char hostName[32];
};

struct MsgRoomCreated
{
    PacketHeader header;
    uint32_t roomId;
};

struct RoomSummaryInfo
{
    uint32_t roomId;
    uint8_t gameMode;  // 0 = Original, 1 = Advance
    uint8_t isPrivate; // 0 = フリー, 1 = プライベート
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
    char password[4];
    char guestName[32];
};

struct MsgJoinRoomResult
{
    PacketHeader header;
    uint8_t success;
    uint8_t reason;
};
#pragma pack(pop)