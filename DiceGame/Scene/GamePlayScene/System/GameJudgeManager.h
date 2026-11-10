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

// 勝敗判定マネージャー定数定義
namespace JudgeConstants
{
    constexpr int INDEX_RETRY = 0; // ダイアログの「RETRY」ボタンインデックス
    constexpr int INDEX_TITLE = 1; // ダイアログの「TITLE」ボタンインデックス
}

// 勝敗判定およびゲーム終了時のダイアログ画面制御マネージャー
class GameJudgeManager
{
public:
    // 勝敗結果の状態
    enum class ResultState
    {
        None, // 試合進行中
        Win,  // 勝利
        Lose, // 敗北
        Draw  // 引き分け
    };

    // ダイアログ上の選択アクション
    enum class Action
    {
        None,
        Retry,    // 再戦
        GoToTitle // タイトルへ戻る
    };

    GameJudgeManager() = default;
    ~GameJudgeManager() = default;

    // 描画バッチやシェーダーリソースを初期化する関数
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ゲーム全体の勝敗状態を評価・判定する関数
    void JudgeGame(const DiceBattleGame& gameLogic, PlayerType myPlayerType);

    // 勝敗結果を手動で設定する関数
    void SetResult(ResultState state)
    {
        m_resultState = state;
    }

    // ゲームセットしたかどうかを取得する関数
    bool IsGameOver() const
    {
        return m_resultState != ResultState::None;
    }

    // 操作更新関数
    Action Update(const DirectX::Mouse::State& mouseState, const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                  const DirectX::Keyboard::KeyboardStateTracker& kbTracker);

    // 描画関数
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                float screenWidth, float screenHeight);

    // 勝敗結果の状態を取得する関数
    ResultState GetResultState() const
    {
        return m_resultState;
    }

    // 選択インデックスを取得する関数
    int GetSelectedIndex() const
    {
        return m_selectIndex;
    }

private:
    ResultState m_resultState = ResultState::None;   // 現在の勝敗状態
    int m_selectIndex = JudgeConstants::INDEX_RETRY; // 選択インデックス

    RECT m_retryButtonRect = {0, 0, 0, 0}; // 「RETRY」ボタンの判定矩形
    RECT m_titleButtonRect = {0, 0, 0, 0}; // 「TITLE」ボタンの判定矩形
    bool m_isRetryHovered = false;         // RETRYボタンホバーフラグ
    bool m_isTitleHovered = false;         // TITLEボタンホバーフラグ

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
};