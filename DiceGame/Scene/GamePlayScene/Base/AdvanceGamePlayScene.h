#pragma once
#include "GamePlaySceneBase.h"
#include "SkillManager.h"

// アドバンスゲームプレイ専用定数定義
namespace AdvanceSceneConstants
{
    constexpr int REVIVE_GRID_Y_MIN = 6;           // 蘇生可能な陣地Y格子最小値
    constexpr int REVIVE_GRID_Y_MAX = 7;           // 蘇生可能な陣地Y格子最大値
    constexpr float FREEZE_NOTICE_DURATION = 1.5f; // 凍結発動時通知の表示時間（秒）
    constexpr int CLICK_PARTICLE_COUNT = 45;       // カードクリック時の粒子演出数
}

// アドバンスモード（スキルカード要素あり）のゲームプレイシーンクラス
class AdvanceGamePlayScene : public GamePlaySceneBase
{
public:
    AdvanceGamePlayScene() = default;
    ~AdvanceGamePlayScene() override = default;

protected:
    // モード固有の初期化（スキルマネージャー初期化、手札生成）を行う関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void OnModeEnter(GameContext& gameContext) override;

    // モード固有の更新（凍結状態判定、スキルカードクリック判定、CPUのAIスキル思考）を行う関数
    // 引数 dt: デルタタイム, gameContext: ゲーム全体の共有コンテキスト
    void OnModeUpdate(float dt, GameContext& gameContext) override;

    // 2D描画（スキルカード手札、重複ダイスモデル、ホバー説明ツールチップ）を行う関数
    // 引数 context: デバイスコンテキスト, screenWidth: 画面幅, screenHeight: 画面高さ, gameContext: 共有コンテキスト
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;

    // 蘇生スキルなどの対象マス選択状態かを判定する関数
    // 戻り値: 選択中であれば true
    bool IsModeSelectingTarget() const override;

    // マス目選択モード時のクリック判定をハンドリングする関数
    // 引数 gridX: 格子X座標, gridY: 格子Y座標
    void OnModeGridClicked(int gridX, int gridY) override;

private:
    SkillManager m_skillManager; // スキルカードおよびデッキの管理マネージャー
};