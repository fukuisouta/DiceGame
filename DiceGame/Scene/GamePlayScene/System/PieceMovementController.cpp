#include "pch.h"
#include "PieceMovementController.h"
#include "GameLogManager.h"
#include "GamePlayUIController.h"
#include "GameContext.h"
#include "GamePlaySceneBase.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 移動制御スタイル定数定義
namespace MovementStyleConstants
{
    constexpr float SHIELD_NOTICE_DURATION = 1.2f; // 守護発動時メッセージの表示時間（秒）
    constexpr int DIRECTION_FORWARD = 1;           // 通常進行方向
    constexpr int DIRECTION_BACKWARD = -1;         // 反転進行方向
}

// 選択状態のリセット
void PieceMovementController::Reset()
{
    m_hasMovedThisTurn = false;
    m_selectedX = MovementConstants::UNSELECTED_INDEX;
    m_selectedY = MovementConstants::UNSELECTED_INDEX;
    m_cursorX = MovementConstants::UNSELECTED_INDEX;
    m_cursorY = MovementConstants::UNSELECTED_INDEX;
}

// 移動可能タイルの取得
std::vector<MovableTileInfo> PieceMovementController::GetMovableTiles(const DiceBattleGame& gameLogic) const
{
    return MovableTileCalculator::Calculate(m_selectedX, m_selectedY, gameLogic.GetRemainingSteps(),
                                            gameLogic.IsWaitingForRoll(), gameLogic);
}

// 移動処理の実行
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
            outNoticeTimer = MovementStyleConstants::SHIELD_NOTICE_DURATION;
            logManager.AddLog(outNoticeText);
            return false;
        }
    }

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

        if (isEnemyDefeated)
        {
            logManager.AddLogFormat(L"%s[目:%d] が %s[目:%d] を撃破！", actor, atkFace, targetName, defFace);
        }
        else
        {
            logManager.AddLogFormat(L"%sがコマ[%d] を (%d, %d) へ移動", actor, atkFace, toX, toY);
        }

        if (!isNetworkReceived && gameContext && gameContext->networkManager.IsConnected())
        {
            gameContext->networkManager.SendMovePiece(fromX, fromY, toX, toY);
        }

        return true;
    }

    return false;
}

// フレーム入力更新
void PieceMovementController::Update(const Mouse::State& mouseState, const Mouse::ButtonStateTracker& mouseTracker,
                                     const Keyboard::KeyboardStateTracker& kbTracker, const Keyboard::State& kbState,
                                     DiceBattleGame& gameLogic, GameLogManager& logManager,
                                     const GamePlayUIController& uiController, int hoveredGridX, int hoveredGridY,
                                     bool isClient, PlayerType myPlayerType, bool isCtrlPressed,
                                     GameContext* gameContext)
{
    (void)mouseState;

    bool canControlPiece =
        !gameLogic.IsWaitingForRoll() && (gameLogic.GetRemainingSteps() > 0) && !gameLogic.IsAnyPieceMoving();

    if (!canControlPiece || logManager.IsButtonHovered() || uiController.IsDiceCheckBtnHovered())
        return;

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

    int moveDx = 0;
    int moveDy = 0;
    if (mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
    {
        if (uiController.IsUpHovered())
            moveDy = isClient ? MovementStyleConstants::DIRECTION_FORWARD : MovementStyleConstants::DIRECTION_BACKWARD;
        if (uiController.IsDownHovered())
            moveDy = isClient ? MovementStyleConstants::DIRECTION_BACKWARD : MovementStyleConstants::DIRECTION_FORWARD;
        if (uiController.IsLeftHovered())
            moveDx = isClient ? MovementStyleConstants::DIRECTION_BACKWARD : MovementStyleConstants::DIRECTION_FORWARD;
        if (uiController.IsRightHovered())
            moveDx = isClient ? MovementStyleConstants::DIRECTION_FORWARD : MovementStyleConstants::DIRECTION_BACKWARD;
    }

    if (!isCtrlPressed)
    {
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Up) || kbTracker.IsKeyPressed(Keyboard::Keys::W))
            moveDy = isClient ? MovementStyleConstants::DIRECTION_FORWARD : MovementStyleConstants::DIRECTION_BACKWARD;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Down) || kbTracker.IsKeyPressed(Keyboard::Keys::S))
            moveDy = isClient ? MovementStyleConstants::DIRECTION_BACKWARD : MovementStyleConstants::DIRECTION_FORWARD;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Left) || kbTracker.IsKeyPressed(Keyboard::Keys::A))
            moveDx = isClient ? MovementStyleConstants::DIRECTION_BACKWARD : MovementStyleConstants::DIRECTION_FORWARD;
        if (kbTracker.IsKeyPressed(Keyboard::Keys::Right) || kbTracker.IsKeyPressed(Keyboard::Keys::D))
            moveDx = isClient ? MovementStyleConstants::DIRECTION_FORWARD : MovementStyleConstants::DIRECTION_BACKWARD;
    }

    if (m_selectedX != MovementConstants::UNSELECTED_INDEX && (moveDx != 0 || moveDy != 0))
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

    if (kbTracker.IsKeyPressed(Keyboard::Keys::Escape) || gameLogic.GetRemainingSteps() <= 0)
    {
        Reset();
    }
}