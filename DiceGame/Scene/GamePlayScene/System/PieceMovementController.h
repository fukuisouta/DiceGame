#pragma once

#include <vector>
#include <string>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <Keyboard.h>
#include "DiceBattleGame.h"
#include "MovableTileCalculator.h"

class GameLogManager;
class GamePlayUIController;
struct GameContext;

// 移動コントローラー定数定義
namespace MovementConstants
{
    constexpr int UNSELECTED_INDEX = -1; // 非選択状態を示す無効インデックス値
}

// プレイヤーのコマ（サイコロ）選択・移動操作・ネットワーク送信の入力コントローラー
class PieceMovementController
{
public:
    PieceMovementController() = default;
    ~PieceMovementController() = default;

    // 選択状態・移動済みフラグをリセットする関数
    void Reset();

    // サイコロの移動ロジック（カーソル選択、キー/UI入力による移動実行）を毎フレーム更新する関数
    // 引数 mouseState: マウス状態, mouseTracker: マウストラッカー, kbTracker: キーボードトラッカー, kbState:
    // キーボード状態,
    //      gameLogic: ゲームロジック参照, logManager: ログマネージャー参照, uiController: UIコントローラー参照,
    //      hoveredGridX, hoveredGridY: ホバー中格子座標, isClient: 2P（クライアント）フラグ, myPlayerType:
    //      自身のプレイヤー種別, isCtrlPressed: Ctrl押下フラグ, gameContext: 共有コンテキストポインタ
    void Update(const DirectX::Mouse::State& mouseState, const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                const DirectX::Keyboard::KeyboardStateTracker& kbTracker, const DirectX::Keyboard::State& kbState,
                DiceBattleGame& gameLogic, GameLogManager& logManager, const GamePlayUIController& uiController,
                int hoveredGridX, int hoveredGridY, bool isClient, PlayerType myPlayerType, bool isCtrlPressed,
                GameContext* gameContext);

    // コマの移動を実行する関数（対戦ログ追加およびネットワーク同期パケット送信を含む）
    // 引数 fromX, fromY: 移動元格子座標, toX, toY: 移動先格子座標, isNetworkReceived: ネットワーク受信経由フラグ,
    //      gameLogic: ゲームロジック参照, logManager: ログマネージャー参照, gameContext: 共有コンテキスト,
    //      outNoticeText: ポップアップ通知文字列参照, outNoticeTimer: ポップアップタイマー参照
    // 戻り値: 移動が成功した場合 true
    bool ExecuteMove(int fromX, int fromY, int toX, int toY, bool isNetworkReceived, DiceBattleGame& gameLogic,
                     GameLogManager& logManager, GameContext* gameContext, std::wstring& outNoticeText,
                     float& outNoticeTimer);

    // 現在選択中のX格子座標を取得する関数
    // 戻り値: 選択中X座標
    int GetSelectedX() const
    {
        return m_selectedX;
    }

    // 現在選択中のY格子座標を取得する関数
    // 戻り値: 選択中Y座標
    int GetSelectedY() const
    {
        return m_selectedY;
    }

    // 現在のカーソルX格子座標を取得する関数
    // 戻り値: カーソルX座標
    int GetCursorX() const
    {
        return m_cursorX;
    }

    // 現在のカーソルY格子座標を取得する関数
    // 戻り値: カーソルY座標
    int GetCursorY() const
    {
        return m_cursorY;
    }

    // このターン中にコマを移動させたかどうかのフラグを取得する関数
    // 戻り値: 移動済みであれば true
    bool HasMovedThisTurn() const
    {
        return m_hasMovedThisTurn;
    }

    // 選択格子座標を明示的に設定する関数
    // 引数 x: X座標, y: Y座標
    void SetSelected(int x, int y)
    {
        m_selectedX = x;
        m_selectedY = y;
    }

    // カーソル格子座標を明示的に設定する関数
    // 引数 x: X座標, y: Y座標
    void SetCursor(int x, int y)
    {
        m_cursorX = x;
        m_cursorY = y;
    }

    // 現在選択中のコマの移動可能マス一覧を取得する関数
    // 引数 gameLogic: ゲームロジック参照
    // 戻り値: 移動可能マス情報の配列
    std::vector<MovableTileInfo> GetMovableTiles(const DiceBattleGame& gameLogic) const;

private:
    int m_cursorX = MovementConstants::UNSELECTED_INDEX;   // ホバー/選択中のカーソルX座標
    int m_cursorY = MovementConstants::UNSELECTED_INDEX;   // ホバー/選択中のカーソルY座標
    int m_selectedX = MovementConstants::UNSELECTED_INDEX; // 現在選択されているコマのX座標
    int m_selectedY = MovementConstants::UNSELECTED_INDEX; // 現在選択されているコマのY座標
    bool m_hasMovedThisTurn = false;                       // このターン中に移動を行ったかどうかのフラグ
};