#pragma once

#include <cstdint>

// ============================================================================
// 通信パケット種別（メッセージID）定義
// ============================================================================
enum class PacketType : uint16_t
{
    None = 0,
    CS_DiceResult, // ダイスの出目同期 (Client -> Server / Peer -> Peer)
    CS_MovePiece,  // コマの移動座標同期
    CS_EndTurn,    // ターン終了通知
    CS_StartGame,  // ゲーム開始通知
    CS_SelectMode, // モード選択通知
    CS_PlayerInfo, // プレイヤー情報の相互同期用
    SC_WorldSync   // ホストからの絶対位置・回転一括同期
};

// メモリ境界のパディングを1バイト詰めに統一（環境間の構造体サイズ不一致を防止）
#pragma pack(push, 1)

// ============================================================================
// パケット共通ヘッダー構造体
// ============================================================================
struct PacketHeader
{
    uint16_t size;   // パケット全体のデータサイズ（バイト数）
    PacketType type; // パケットの識別タイプ
};

// プレイヤー名情報パケット
struct MsgPlayerInfo
{
    PacketHeader header;
    char playerName[64]; // プレイヤー表示名
};

// ダイス出目同期パケット
struct MsgDiceResult
{
    PacketHeader header;
    int steps; // 決定された移動歩数
};

// コマ移動データパケット
struct MsgMovePiece
{
    PacketHeader header;
    int fromX, fromY; // 移動元格子座標
    int toX, toY;     // 移動先格子座標
};

// モード選択通知パケット
struct MsgSelectMode
{
    PacketHeader header;
    uint8_t mode; // 選択されたゲームモード
};

// 1つの3Dオブジェクトの変換行列（位置・回転）データ
struct TransformData
{
    float posX, posY, posZ;       // ワールド位置座標
    float rotX, rotY, rotZ, rotW; // 姿勢回転クォータニオン
};

// 1つのコマの同期用データ構造体
struct PieceSyncData
{
    int8_t gridX;            // 格子位置X
    int8_t gridY;            // 格子位置Y
    uint8_t owner;           // 所有プレイヤー (Player1 / Player2)
    uint8_t type;            // コマ種別 (King / Pawn)
    uint8_t isActive;        // 活性フラグ（1バイト整数としてパディングズレを防ぐ）
    TransformData transform; // 3D姿勢データ
};

// 盤面全体・全コマの絶対座標を一括同期するワールドパケット（ホスト -> クライアント）
struct MsgWorldSync
{
    PacketHeader header;
    TransformData stepDiceTransform; // ステップダイスの現在の3D姿勢
    uint8_t pieceCount;              // 同期対象のコマの総数
    PieceSyncData pieces[10];        // 全コマの同期データ配列（最大10個）
};

#pragma pack(pop)