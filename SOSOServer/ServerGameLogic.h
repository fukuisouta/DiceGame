#pragma once
#pragma once

#include "GameConfig.h"
#include <vector>
#include <cstdint>
#include <random>

// サーバー上で管理するプレイヤー種別列挙型
enum class ServerPlayerType : uint8_t
{
    None = 0, // 未割り当て
    Player1,  // 1P (ホスト役 / 盤面下側)
    Player2   // 2P (クライアント役 / 盤面上側)
};

// サーバー上で管理するコマのデータ構造体
struct ServerBoardPiece
{
    ServerPlayerType owner = ServerPlayerType::None; // コマの所有プレイヤー
    uint8_t type = 0;                                // コマの種類 (0: King, 1: Pawn)
    int8_t gridX = 0;                                // 現在の格子X座標
    int8_t gridY = 0;                                // 現在の格子Y座標
    bool isActive = true;                            // 生存フラグ
    uint8_t topFace = 1;                             // 現在の上面の出目 (1〜6)
};

// サーバー権限でゲームの進行・盤面・ルール判定を行うクラス
class ServerGameLogic
{
public:
    ServerGameLogic() = default;
    ~ServerGameLogic() = default;

    // 設定情報に基づき盤面とコマを初期配置する関数
    // 引数 config: 盤面設定構造体
    void Initialize(const BoardConfig& config = BoardConfig());

    // 歩数ダイスの出目をサーバー上で安全に生成する関数
    // 戻り値: 決定した出目
    int RollStepDice();

    // 移動コマンドの妥当性を検証し実行する関数
    // 引数 player: 移動を行うプレイヤー, fromX/fromY: 移動元, toX/toY: 移動先
    // 戻り値: 成功時 true
    bool ValidateAndExecuteMove(ServerPlayerType player, int fromX, int fromY, int toX, int toY);

    // ゲーム終了判定を行う関数
    // 戻り値: 対局終了時 true
    bool CheckGameOver();

    ServerPlayerType GetCurrentTurn() const { return m_currentTurn; }
    ServerPlayerType GetWinner() const { return m_winner; }
    int GetRemainingSteps() const { return m_remainingSteps; }
    const std::vector<ServerBoardPiece>& GetPieces() const { return m_pieces; }

private:
    void ResolveBattle(size_t attackerIdx, size_t defenderIdx);
    void SwitchTurn();

private:
    BoardConfig m_config;
    std::vector<ServerBoardPiece> m_pieces;
    ServerPlayerType m_currentTurn = ServerPlayerType::Player1;
    int m_remainingSteps = 0;
    bool m_isGameOver = false;
    ServerPlayerType m_winner = ServerPlayerType::None;
    std::mt19937 m_rng{ std::random_device{}() };
};