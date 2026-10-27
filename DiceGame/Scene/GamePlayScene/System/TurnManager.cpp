#include "pch.h"
#include "TurnManager.h"
#include "GameLogManager.h"

// 初期化処理
void TurnManager::Initialize(PlayerType initialTurn)
{
    m_previousTurn = initialTurn;
    m_noticeText = L"";
    m_noticeTimer = 0.0f;
}

// 画面中央への通知ポップアップ発動
void TurnManager::TriggerNotice(const std::wstring& text, float duration, GameLogManager* logManager)
{
    m_noticeText = text;
    m_noticeMaxTime = duration;
    m_noticeTimer = duration;

    if (logManager)
    {
        logManager->AddLog(text.c_str());
    }
}

// 前フレームからのターン切り替え検知
bool TurnManager::HasTurnChanged(PlayerType currentTurn)
{
    if (currentTurn != m_previousTurn)
    {
        m_previousTurn = currentTurn;
        return true;
    }
    return false;
}

// 毎フレームの更新処理（テロップタイマー減衰、ターン交替検知、オフラインCPUの思考・移動）
void TurnManager::Update(float dt, DiceBattleGame& gameLogic, DiceBattleAI& ai, GameLogManager& logManager,
                         bool isNetworkConnected)
{
    // ターン通知タイマーの減衰
    if (m_noticeTimer > 0.0f)
    {
        m_noticeTimer -= dt;
        if (m_noticeTimer < 0.0f)
            m_noticeTimer = 0.0f;
    }

    // ターン切り替え検知とテロップ通知
    PlayerType currentTurn = gameLogic.GetCurrentTurn();
    if (HasTurnChanged(currentTurn))
    {
        gameLogic.SetRemainingSteps(0);
        std::wstring notice = (currentTurn == PlayerType::Player1) ? L"あなたのターン" : L"あいてのターン";
        TriggerNotice(notice, 1.5f, &logManager);
    }

    // オフライン（ローカル）プレイ時のCPU（Player2）思考および移動実行
    if (!isNetworkConnected && currentTurn == PlayerType::Player2)
    {
        gameLogic.SetPlayerShield(PlayerType::Player2, false);
        int stepsBefore = gameLogic.GetRemainingSteps();

        ai.Update(gameLogic, dt);

        if (gameLogic.GetRemainingSteps() < stepsBefore && stepsBefore > 0)
        {
            logManager.AddLog(L"あいて(CPU)がコマを移動しました");
        }
    }
}