#pragma once

#include <string>
#include "DiceBattleGame.h"
#include "DiceBattleAI.h"

class GameLogManager;

// ============================================================================
// ターン管理クラス（手番切り替え判定・テロップ通知タイマー・オフラインCPU手番の統合）
// ============================================================================
class TurnManager
{
public:
    TurnManager() = default;
    ~TurnManager() = default;

    // 初期化（開始ターンの設定）
    void Initialize(PlayerType initialTurn = PlayerType::Player1);

    // 毎フレームのターン状態監視・テロップタイマー処理・CPU思考処理の統合更新
    void Update(float dt, DiceBattleGame& gameLogic, DiceBattleAI& ai, GameLogManager& logManager,
                bool isNetworkConnected);

    // 手動でターン切り替え通知（テロップ）を発生
    void TriggerNotice(const std::wstring& text, float duration = 1.5f, GameLogManager* logManager = nullptr);

    // --- ゲッター ---
    bool IsNoticeActive() const
    {
        return m_noticeTimer > 0.0f;
    }
    const std::wstring& GetNoticeText() const
    {
        return m_noticeText;
    }
    float GetNoticeTimer() const
    {
        return m_noticeTimer;
    }

private:
    // ターンが切り替わったかを検知する内部ヘルパー
    bool HasTurnChanged(PlayerType currentTurn);

private:
    PlayerType m_previousTurn = PlayerType::Player1; // 前フレームのターン状態
    std::wstring m_noticeText = L"";                 // 通知メッセージ文字列
    float m_noticeTimer = 0.0f;                      // 通知テロップの残り表示時間
    float m_noticeMaxTime = 1.5f;                    // 通知テロップの最大表示時間
};