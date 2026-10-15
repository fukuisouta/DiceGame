#pragma once
#include <cstdint>

enum class PacketType : uint16_t
{
    None = 0,
    SC_DiceResult, // ダイスの出目同期
    CS_MovePiece,  // コマの移動同期
    CS_EndTurn,    // ターン終了通知
    CS_StartGame,  // ゲーム開始通知
    CS_SelectMode, // モード選択通知
    SC_WorldSync   // ★ 追加: ホストからの絶対位置・回転一括同期
};

#pragma pack(push, 1)

struct PacketHeader
{
    uint16_t size;
    PacketType type;
};

struct MsgDiceResult
{
    PacketHeader header;
    int steps;
};

struct MsgMovePiece
{
    PacketHeader header;
    int fromX, fromY;
    int toX, toY;
};

struct MsgSelectMode
{
    PacketHeader header;
    uint8_t mode;
};

// ★ 追加: 1つのオブジェクトの位置(Vector3)と回転(Quaternion)
struct TransformData
{
    float posX, posY, posZ;
    float rotX, rotY, rotZ, rotW;
};

// ★ 追加: ステップダイスと全コマの位置・角度を一括で送るパケット
struct MsgWorldSync
{
    PacketHeader header;
    TransformData stepDiceTransform; // ステップダイスの姿勢
    uint8_t pieceCount;              // 送信するコマの数
    struct PieceSyncData
    {
        uint8_t owner; // Player1 / Player2
        uint8_t type;  // King / Pawn
        int8_t gridX;
        int8_t gridY;
        bool isActive;
        TransformData transform;
    } pieces[10];
};

#pragma pack(pop)