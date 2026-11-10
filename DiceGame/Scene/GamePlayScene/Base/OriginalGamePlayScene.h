#pragma once

#include "GamePlaySceneBase.h"

// オリジナルゲームプレイ専用定数定義
namespace OriginalSceneConstants
{
    constexpr float THINKING_TEXT_POS_X_OFFSET = 110.0f; // 相手思考中メッセージのX軸表示オフセット
    constexpr float THINKING_TEXT_POS_Y = 20.0f;         // 相手思考中メッセージのY軸表示位置
}

// オリジナルモード（ネットワーク同期・クラシックルール）のゲームプレイシーンクラス
class OriginalGamePlayScene : public GamePlaySceneBase
{
public:
    OriginalGamePlayScene() = default;
    ~OriginalGamePlayScene() override = default;

protected:
    // オンラインコールバックの登録とモード開始を行う関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void OnModeEnter(GameContext& gameContext) override;

    // パケット同期送信およびターン管理を行う関数
    // 引数 dt: デルタタイム, gameContext: ゲーム全体の共有コンテキスト
    void OnModeUpdate(float dt, GameContext& gameContext) override;

    // 相手思考中のオーバーレイテキストメッセージを描画する関数
    // 引数 context: デバイスコンテキスト, screenWidth: 画面幅, screenHeight: 画面高さ, gameContext: 共有コンテキスト
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;

    // ターゲット選択モード判定（オリジナルモードは常に通常状態）
    // 戻り値: 常に false
    bool IsModeSelectingTarget() const override
    {
        return false;
    }

    // マス目クリック判定（オリジナルモードでは特殊なマス操作はなし）
    void OnModeGridClicked(int gridX, int gridY) override
    {
        (void)gridX;
        (void)gridY;
    }

private:
    // 自身が現在操作権利（手番）を持っているかを判定するヘルパー関数
    // 引数 gameContext: 共有コンテキスト
    // 戻り値: 自分の手番であれば true, 相手の手番であれば false
    bool IsMyTurn(GameContext& gameContext) const;
};