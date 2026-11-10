#pragma once

#include <string>
#include "DiceBattleGame.h"
#include "DiceBattleAI.h"

class GameLogManager;

// ターンマネージャー定数定義
namespace TurnManagerConstants
{
    constexpr float DEFAULT_NOTICE_DURATION = 1.5f; // 標準の通知テロップ表示時間（秒）
}

// ターン管理クラス（手番切り替え判定・テロップ通知タイマー・オフラインCPU手番の統合）
class TurnManager
{
public:
    TurnManager() = default;
    ~TurnManager() = default;

    // 開始ターンの設定を行い初期化する関数
    // 引数 initialTurn: 初期手番プレイヤー
    void Initialize(PlayerType initialTurn = PlayerType::Player1);

    // 毎フレームのターン状態監視・テロップタイマー処理・CPU思考処理を統合更新する関数
    // 引数 dt: デルタタイム, gameLogic: ゲームロジック参照, ai: AI思考ロジック参照, logManager: ログマネージャー参照,
    // isNetworkConnected: ネットワーク接続中フラグ
    void Update(float dt, DiceBattleGame& gameLogic, DiceBattleAI& ai, GameLogManager& logManager,
                bool isNetworkConnected);

    // 手動でターン切り替え通知（テロップ）を発生させる関数
    // 引数 text: 通知文字列, duration: 表示時間（秒）, logManager: ログマネージャーポインタ
    void TriggerNotice(const std::wstring& text, float duration = TurnManagerConstants::DEFAULT_NOTICE_DURATION,
                       GameLogManager* logManager = nullptr);

    // 通知テロップが画面上に表示中かどうかを取得する関数
    // 戻り値: 表示中であれば true
    bool IsNoticeActive() const
    {
        return m_noticeTimer > 0.0f;
    }

    // 現在表示中の通知メッセージ文字列を取得する関数
    // 戻り値: 通知メッセージ参照
    const std::wstring& GetNoticeText() const
    {
        return m_noticeText;
    }

    // 通知テロップの残り表示時間を取得する関数
    // 戻り値: 残り時間（秒）
    float GetNoticeTimer() const
    {
        return m_noticeTimer;
    }

private:
    // 前フレームから手番が切り替わったかを検知する内部ヘルパー関数
    // 引数 currentTurn: 現在の手番
    // 戻り値: 切り替わった直後であれば true
    bool HasTurnChanged(PlayerType currentTurn);

private:
    PlayerType m_previousTurn = PlayerType::Player1;                       // 前フレームの手番状態
    std::wstring m_noticeText = L"";                                       // 通知メッセージ文字列
    float m_noticeTimer = 0.0f;                                            // 通知テロップの残り表示時間（秒）
    float m_noticeMaxTime = TurnManagerConstants::DEFAULT_NOTICE_DURATION; // 通知テロップの最大表示時間（秒）
};