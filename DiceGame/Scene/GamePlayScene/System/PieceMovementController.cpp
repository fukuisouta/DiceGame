#include "pch.h"
#include "PieceMovementController.h"
#include "GameLogManager.h"
#include "GamePlayUIController.h"
#include "GameContext.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 選択状態のリセット
void PieceMovementController::Reset()
{
    m_hasMovedThisTurn = false;
    m_selectedX = -1;
    m_selectedY = -1;
    m_cursorX = -1;
    m_cursorY = -1;
}

// 移動可能タイルの取得
std::vector<MovableTileInfo> PieceMovementController::GetMovableTiles(const DiceBattleGame& gameLogic) const
{
    return MovableTileCalculator::Calculate(m_selectedX, m_selectedY, gameLogic.GetRemainingSteps(),
                                            gameLogic.IsWaitingForRoll(), gameLogic);
}

// 移動処理の実行（ログ登録・通信送信・守護スキル判定等を含む）
bool PieceMovementController::ExecuteMove(int fromX, int fromY, int toX, int toY, bool isNetworkReceived,
                                          DiceBattleGame& gameLogic, GameLogManager& logManager,
                                          GameContext* gameContext, std::wstring& outNoticeText, float& outNoticeTimer)
{
    PlayerType currentTurn = gameLogic.GetCurrentTurn();

    const auto* attackerPiece = gameLogic.GetPieceAt(fromX, fromY);
    const auto* targetPiece = gameLogic.GetPieceAt(toX, toY);

    int atkFace = gameLogic.GetPieceTopFace(attackerPiece);
    int defFace = gameLogic.GetPieceTopFace(targetPiece);

    bool isEnemyDefeated = (targetPiece != nullptr && targetPiece->owner != currentTurn);

    // 敵コマへの攻撃時にシールド（守護スキル）が有効な場合のガード判定
    if (targetPiece && targetPiece->owner != currentTurn)
    {
        if (gameLogic.IsPlayerShielded(targetPiece->owner))
        {
            int remaining = gameLogic.GetRemainingSteps() - 1;
            if (remaining <= 0)
            {
                gameLogic.SetRemainingSteps(0);
                gameLogic.ForceEndTurn();
            }
            else
            {
                gameLogic.SetRemainingSteps(remaining);
            }

            outNoticeText = L"相手の「守護」により防がれた";
            outNoticeTimer = 1.2f;
            logManager.AddLog(outNoticeText);
            return false;
        }
    }

    // ゲームロジック側での実際の座標・回転変更と勝敗評価の実行
    if (gameLogic.MovePiece(fromX, fromY, toX, toY))
    {
        m_hasMovedThisTurn = true;
        m_selectedX = toX;
        m_selectedY = toY;
        m_cursorX = toX;
        m_cursorY = toY;

        bool isClientMode = (gameContext && gameContext->networkManager.IsConnected() &&
                             gameContext->networkManager.GetMode() == NetworkManager::Mode::Client);
        PlayerType myPlayerType = isClientMode ? PlayerType::Player2 : PlayerType::Player1;

        const wchar_t* actor = (currentTurn == myPlayerType) ? L"あなた" : L"あいて";
        const wchar_t* targetName = (currentTurn == myPlayerType) ? L"あいて" : L"あなた";

        // 行動結果に応じたログ文章の生成
        if (isEnemyDefeated)
        {
            logManager.AddLogFormat(L"%s[目:%d] が %s[目:%d] を撃破！", actor, atkFace, targetName, defFace);
        }
        else
        {
            logManager.AddLogFormat(L"%sがコマ[%d] を (%d, %d) へ移動", actor, atkFace, toX, toY);
        }

        // 自分が操作した移動であれば対戦相手に移動パケットを送信
        if (!isNetworkReceived && gameContext && gameContext->networkManager.IsConnected())
        {
            gameContext->networkManager.SendMovePiece(fromX, fromY, toX, toY);
        }

        return true;
    }

    return false;
}

// 入力イベント（マウス・キーボード・UI）に応じたコマの選択・移動更新
void PieceMovementController::Update(const Mouse::State& mouseState, const Mouse::ButtonStateTracker& mouseTracker,
                                     const Keyboard::KeyboardStateTracker& kbTracker, const Keyboard::State& kbState,
                                     DiceBattleGame& gameLogic, GameLogManager& logManager,
                                     const GamePlayUIController& uiController, int hoveredGridX, int hoveredGridY,
                                     bool isClient, PlayerType myPlayerType, bool isCtrlPressed,
                                     GameContext* gameContext)
{
    bool canControlPiece =
        !gameLogic.IsWaitingForRoll() && (gameLogic.GetRemainingSteps() > 0) && !gameLogic.IsAnyPieceMoving();

    if (!canControlPiece || logManager.IsButtonHovered() || uiController.IsDiceCheckBtnHovered())
        return;

    // マウスクリックによる自陣コマの選択
    if (mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED && !uiController.IsRollButtonHovered() &&
        !uiController.IsUpHovered() && !uiController.IsDownHovered() && !uiController.IsLeftHovered() &&
        !uiController.IsRightHovered())
    {
        if (hoveredGridX >= 0 && hoveredGridX < DiceBattleGame::BOARD_WIDTH && hoveredGridY >= 0 &&
            hoveredGridY < DiceBattleGame::BOARD_HEIGHT)
        {
            const auto* piece = gameLogic.GetPieceAt(hoveredGridX, hoveredGridY);
            if (piece && piece->owner == myPlayerType)
            {
                m_cursorX = hoveredGridX;
                m_cursorY = hoveredGridY;
                m_selectedX = hoveredGridX;
                m_selectedY = hoveredGridY;
            }
        }
    }

    // UIボタン操作による移動方向の入力判定（Client視点の場合は反転）
    int moveDx = 0, moveDy = 0;
    if (mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
    {
        if (uiController.IsUpHovered())
            moveDy = isClient ? 1 : -1;
        if (uiController.IsDownHovered())
            moveDy = isClient ? -1 : 1;
        if (uiController.IsLeftHovered())
            moveDx = isClient ? -1 : 1;
        if (uiController.IsRightHovered())
            moveDx = isClient ? 1 : -1;
    }

    // キーボード（WASD / 矢印キー）による移動方向の入力判定
    if (!isCtrlPressed)
    {
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Up) || kbTracker.IsKeyPressed(Keyboard::Keys::W))
            moveDy = isClient ? 1 : -1;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Down) || kbTracker.IsKeyPressed(Keyboard::Keys::S))
            moveDy = isClient ? -1 : 1;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Left) || kbTracker.IsKeyPressed(Keyboard::Keys::A))
            moveDx = isClient ? -1 : 1;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Right) || kbTracker.IsKeyPressed(Keyboard::Keys::D))
            moveDx = isClient ? 1 : -1;
    }

    // 移動先の妥当性を確認のうえ移動を実行
    if (m_selectedX != -1 && (moveDx != 0 || moveDy != 0))
    {
        int targetX = m_selectedX + moveDx;
        int targetY = m_selectedY + moveDy;

        auto validTiles = GetMovableTiles(gameLogic);
        bool isValidMove = false;
        for (const auto& tile : validTiles)
        {
            if (tile.gridX == targetX && tile.gridY == targetY)
            {
                isValidMove = true;
                break;
            }
        }

        if (isValidMove)
        {
            std::wstring noticeText;
            float noticeTimer = 0.0f;
            ExecuteMove(m_selectedX, m_selectedY, targetX, targetY, false, gameLogic, logManager, gameContext,
                        noticeText, noticeTimer);
        }
    }

    // ESCキーまたは歩数切れによる選択解除
    if (kbTracker.IsKeyPressed(Keyboard::Keys::Escape) || gameLogic.GetRemainingSteps() <= 0)
    {
        Reset();
    }
}