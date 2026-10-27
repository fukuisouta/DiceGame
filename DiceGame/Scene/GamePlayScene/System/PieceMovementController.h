#pragma once

#include <vector>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <Keyboard.h>
#include "DiceBattleGame.h"
#include "MovableTileCalculator.h"

class GameLogManager;
class GamePlayUIController;
struct GameContext;

// ============================================================================
// プレイヤーのコマ（サイコロ）選択・移動操作・ネットワーク送信の入力コントローラー
// ============================================================================
class PieceMovementController
{
public:
    PieceMovementController() = default;
    ~PieceMovementController() = default;

    // 選択状態・移動済みフラグのリセット
    void Reset();

    // サイコロの移動ロジック更新（カーソル選択、キー/UI入力による移動実行）
    void Update(const DirectX::Mouse::State& mouseState, const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                const DirectX::Keyboard::KeyboardStateTracker& kbTracker, const DirectX::Keyboard::State& kbState,
                DiceBattleGame& gameLogic, GameLogManager& logManager, const GamePlayUIController& uiController,
                int hoveredGridX, int hoveredGridY, bool isClient, PlayerType myPlayerType, bool isCtrlPressed,
                GameContext* gameContext);

    // コマの移動実行（対戦ログ追加およびネットワーク同期パケット送信を含む）
    bool ExecuteMove(int fromX, int fromY, int toX, int toY, bool isNetworkReceived, DiceBattleGame& gameLogic,
                     GameLogManager& logManager, GameContext* gameContext, std::wstring& outNoticeText,
                     float& outNoticeTimer);

    // --- ゲッター / セッター ---
    int GetSelectedX() const
    {
        return m_selectedX;
    }
    int GetSelectedY() const
    {
        return m_selectedY;
    }
    int GetCursorX() const
    {
        return m_cursorX;
    }
    int GetCursorY() const
    {
        return m_cursorY;
    }
    bool HasMovedThisTurn() const
    {
        return m_hasMovedThisTurn;
    }

    void SetSelected(int x, int y)
    {
        m_selectedX = x;
        m_selectedY = y;
    }
    void SetCursor(int x, int y)
    {
        m_cursorX = x;
        m_cursorY = y;
    }

    // 現在選択中のコマの移動可能マス一覧を取得
    std::vector<MovableTileInfo> GetMovableTiles(const DiceBattleGame& gameLogic) const;

private:
    int m_cursorX = -1;              // ホバー/選択中のカーソルX座標
    int m_cursorY = -1;              // ホバー/選択中のカーソルY座標
    int m_selectedX = -1;            // 現在選択されているコマのX座標
    int m_selectedY = -1;            // 現在選択されているコマのY座標
    bool m_hasMovedThisTurn = false; // このターン中に移動を行ったかどうかのフラグ
};