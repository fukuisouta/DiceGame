#pragma once

#include <memory>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <Keyboard.h>
#include "DiceBattleGame.h"

namespace Imase
{
    class DebugRenderer;
}

// ============================================================================
// 勝敗判定およびゲーム終了時のダイアログ画面制御マネージャー
// ============================================================================
class GameJudgeManager
{
public:
    enum class ResultState
    {
        None, // 試合進行中
        Win,  // 勝利
        Lose, // 敗北
        Draw  // 引き分け
    };

    enum class Action
    {
        None,
        Retry,    // 再戦
        GoToTitle // タイトルへ戻る
    };

    GameJudgeManager() = default;
    ~GameJudgeManager() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ゲーム全体の勝敗状態を評価・判定
    void JudgeGame(const DiceBattleGame& gameLogic, PlayerType myPlayerType);

    // 手動で勝敗結果を設定（デバッグショートカット用）
    void SetResult(ResultState state)
    {
        m_resultState = state;
    }

    // ゲームセット判定
    bool IsGameOver() const
    {
        return m_resultState != ResultState::None;
    }

    // 勝敗ダイアログ画面でのマウス/キー操作更新
    Action Update(const DirectX::Mouse::State& mouseState, const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                  const DirectX::Keyboard::KeyboardStateTracker& kbTracker);

    // 勝敗ダイアログおよびボタンのレンダリング
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                float screenWidth, float screenHeight);

    ResultState GetResultState() const
    {
        return m_resultState;
    }
    int GetSelectedIndex() const
    {
        return m_selectIndex;
    }

private:
    ResultState m_resultState = ResultState::None;
    int m_selectIndex = 0; // 0: RETRY, 1: TITLE

    RECT m_retryButtonRect = {0, 0, 0, 0};
    RECT m_titleButtonRect = {0, 0, 0, 0};
    bool m_isRetryHovered = false;
    bool m_isTitleHovered = false;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
};