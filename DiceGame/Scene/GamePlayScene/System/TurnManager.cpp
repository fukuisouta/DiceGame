#include "pch.h"
#include "TurnManager.h"
#include "GameLogManager.h"
#include <vector>

namespace
{
    // 盤面スナップショット用のコマ情報
    struct PieceSnapshot
    {
        int x = -1;
        int y = -1;
        PlayerType owner = PlayerType::Player1;
        PieceType type = PieceType::Pawn;
        int topFace = 0;
    };

    // 現在の盤面上の生存コマ一覧を取得
    std::vector<PieceSnapshot> CaptureBoardSnapshot(const DiceBattleGame& gameLogic)
    {
        std::vector<PieceSnapshot> list;
        for (const auto& p : gameLogic.GetPieces())
        {
            if (!p.isActive)
                continue;

            PieceSnapshot snap;
            snap.x = p.gridX;
            snap.y = p.gridY;
            snap.owner = p.owner;
            snap.type = p.type;
            snap.topFace = gameLogic.GetPieceTopFace(&p);
            list.push_back(snap);
        }
        return list;
    }

    // コマの表示名を生成（「王」または「コマ[出目]」）
    std::wstring FormatPieceLabel(PieceType type, int topFace)
    {
        if (type == PieceType::King)
        {
            return L"王";
        }
        return L"コマ[" + std::to_wstring(topFace) + L"]";
    }
}

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

// 毎フレームの更新処理
void TurnManager::Update(float dt, DiceBattleGame& gameLogic, DiceBattleAI& ai, GameLogManager& logManager,
                         bool isNetworkConnected, PlayerType myPlayerType)
{
    if (m_noticeTimer > 0.0f)
    {
        m_noticeTimer -= dt;
        if (m_noticeTimer < 0.0f)
        {
            m_noticeTimer = 0.0f;
        }
    }

    // 前フレームの残り歩数を記憶して、ダイスが着地した瞬間（0 -> 1〜3）を検出する
    static int s_lastKnownSteps = 0;

    PlayerType currentTurn = gameLogic.GetCurrentTurn();
    if (HasTurnChanged(currentTurn))
    {
        //gameLogic.SetRemainingSteps(0);
        s_lastKnownSteps = 0;
        std::wstring notice = (currentTurn == myPlayerType) ? L"あなたのターン" : L"あいてのターン";
        TriggerNotice(notice, TurnManagerConstants::DEFAULT_NOTICE_DURATION, &logManager);
    }

    // オフラインCPU対戦時のみ、ダイスが振り終わって残り歩数が 0 -> 1以上 に増えた瞬間を検出してログ出力
    int currentSteps = gameLogic.GetRemainingSteps();
    if (!isNetworkConnected && s_lastKnownSteps == 0 && currentSteps > 0)
    {
        if (currentTurn == PlayerType::Player2)
        {
            logManager.AddLogFormat(L"あいてがダイスを振り【%d】が出ました", currentSteps);
        }
    }
    s_lastKnownSteps = currentSteps;

    if (!isNetworkConnected && currentTurn == PlayerType::Player2)
    {
        gameLogic.SetPlayerShield(PlayerType::Player2, false);

        int stepsBefore = gameLogic.GetRemainingSteps();
        auto boardBefore = CaptureBoardSnapshot(gameLogic);

        ai.Update(gameLogic, dt);

        int stepsAfter = gameLogic.GetRemainingSteps();
        s_lastKnownSteps = stepsAfter;

        // CPUがコマを移動した瞬間を検出して詳細ログを出力
        if (stepsAfter < stepsBefore && stepsBefore > 0)
        {
            auto boardAfter = CaptureBoardSnapshot(gameLogic);

            PieceSnapshot movedFrom{};
            PieceSnapshot movedTo{};
            PieceSnapshot defeatedPlayerPiece{};
            bool foundFrom = false;
            bool foundTo = false;
            bool foundDefeatedPlayer = false;

            // 1. 移動元を特定
            for (const auto& before : boardBefore)
            {
                if (before.owner != PlayerType::Player2)
                    continue;

                bool stillThere = false;
                for (const auto& after : boardAfter)
                {
                    if (after.owner == PlayerType::Player2 && after.x == before.x && after.y == before.y)
                    {
                        stillThere = true;
                        break;
                    }
                }
                if (!stillThere)
                {
                    movedFrom = before;
                    foundFrom = true;
                    break;
                }
            }

            // 2. 移動先を特定
            for (const auto& after : boardAfter)
            {
                if (after.owner != PlayerType::Player2)
                    continue;

                bool wasThere = false;
                for (const auto& before : boardBefore)
                {
                    if (before.owner == PlayerType::Player2 && before.x == after.x && before.y == after.y)
                    {
                        wasThere = true;
                        break;
                    }
                }
                if (!wasThere)
                {
                    movedTo = after;
                    foundTo = true;
                    break;
                }
            }

            // 3. 移動先に元々あなたのコマがいたか（撃破したか）を確認
            if (foundTo)
            {
                for (const auto& before : boardBefore)
                {
                    if (before.owner == PlayerType::Player1 && before.x == movedTo.x && before.y == movedTo.y)
                    {
                        defeatedPlayerPiece = before;
                        foundDefeatedPlayer = true;
                        break;
                    }
                }
            }

            if (foundFrom && foundTo)
            {
                std::wstring pieceLabel = FormatPieceLabel(movedTo.type, movedTo.topFace);

                if (foundDefeatedPlayer)
                {
                    std::wstring targetLabel =
                        L"あなたの" + FormatPieceLabel(defeatedPlayerPiece.type, defeatedPlayerPiece.topFace);

                    logManager.AddLogFormat(L"あいての%sが (%d,%d) -> (%d,%d) で%sを撃破！", pieceLabel.c_str(),
                                            movedFrom.x, movedFrom.y, movedTo.x, movedTo.y, targetLabel.c_str());
                }
                else
                {
                    logManager.AddLogFormat(L"あいてが%sを (%d,%d) -> (%d,%d) へ移動（残り%d歩）", pieceLabel.c_str(),
                                            movedFrom.x, movedFrom.y, movedTo.x, movedTo.y, stepsAfter);
                }
            }
            else if (foundFrom && !foundTo)
            {
                std::wstring pieceLabel = FormatPieceLabel(movedFrom.type, movedFrom.topFace);
                logManager.AddLogFormat(L"あいての%sが攻撃を仕掛けたが返り討ちに遭い消滅！", pieceLabel.c_str());
            }
            else
            {
                logManager.AddLogFormat(L"あいてがコマを移動しました（残り%d歩）", stepsAfter);
            }
        }
    }
}