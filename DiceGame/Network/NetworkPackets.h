#pragma once

#include <cstdint>

// ネットワークパケット全般の定数定義
namespace PacketConstants
{
    constexpr size_t PLAYER_NAME_MAX_LEN = 64; // プレイヤー名の最大文字列長
    constexpr size_t MAX_SYNC_PIECES = 10;     // 一括同期対象となるコマの最大個数
}

// 通信パケット種別（メッセージID）定義
enum class PacketType : uint16_t
{
    None = 0,      // 未定義パケット
    CS_DiceResult, // ダイスの出目同期 (Client -> Server / Peer -> Peer)
    CS_MovePiece,  // コマの移動座標同期
    CS_EndTurn,    // ターン終了通知
    CS_StartGame,  // ゲーム開始通知
    CS_SelectMode, // モード選択通知
    CS_PlayerInfo, // プレイヤー情報の相互同期用
    SC_WorldSync   // ホストからの絶対位置・回転一括同期
};

// メモリ境界のパディングを1バイト詰めに統一
#pragma pack(push, 1)

// パケット共通ヘッダー構造体
struct PacketHeader
{
    uint16_t size;   // パケット全体のデータサイズ (バイト数)
    PacketType type; // パケットの識別タイプ
};

// プレイヤー名情報パケット
struct MsgPlayerInfo
{
    PacketHeader header;                                   // 共通パケットヘッダー
    char playerName[PacketConstants::PLAYER_NAME_MAX_LEN]; // プレイヤー表示名バッファ
};

// ダイス出目同期パケット
struct MsgDiceResult
{
    PacketHeader header; // 共通パケットヘッダー
    int steps;           // 決定された移動歩数
};

// コマ移動データパケット
struct MsgMovePiece
{
    PacketHeader header; // 共通パケットヘッダー
    int fromX;           // 移動元の格子X座標
    int fromY;           // 移動元の格子Y座標
    int toX;             // 移動先の格子X座標
    int toY;             // 移動先の格子Y座標
};

// モード選択通知パケット
struct MsgSelectMode
{
    PacketHeader header; // 共通パケットヘッダー
    uint8_t mode;        // 選択されたゲームモード識別値
};

// 1つの3Dオブジェクトの変換（位置・回転）データ構造体
struct TransformData
{
    float posX; // ワールド空間上のX座標
    float posY; // ワールド空間上のY座標
    float posZ; // ワールド空間上のZ座標
    float rotX; // 姿勢回転クォータニオンのX成分
    float rotY; // 姿勢回転クォータニオンのY成分
    float rotZ; // 姿勢回転クォータニオンのZ成分
    float rotW; // 姿勢回転クォータニオンのW成分
};

// 1つのコマの同期用データ構造体
struct PieceSyncData
{
    int8_t gridX;            // 格子位置X
    int8_t gridY;            // 格子位置Y
    uint8_t owner;           // 所有プレイヤー (Player1 / Player2)
    uint8_t type;            // コマ種別 (King / Pawn 等)
    uint8_t isActive;        // 活性フラグ (1:生存, 0:撃破)
    TransformData transform; // 3D姿勢データ
};

// 盤面全体・全コマの絶対座標を一括同期するワールドパケット（ホスト -> クライアント）
struct MsgWorldSync
{
    PacketHeader header;                                    // 共通パケットヘッダー
    TransformData stepDiceTransform;                        // ステップダイスの現在の3D姿勢
    uint8_t pieceCount;                                     // 同期対象コマの実際の個数
    PieceSyncData pieces[PacketConstants::MAX_SYNC_PIECES]; // 全コマの同期データ配列
};

#pragma pack(pop)