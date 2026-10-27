#pragma once
#include "GamePlaySceneBase.h"
#include "SkillManager.h"

// ============================================================================
// アドバンスモード（スキルカード要素あり）のゲームプレイシーン
// ============================================================================
class AdvanceGamePlayScene : public GamePlaySceneBase
{
public:
    AdvanceGamePlayScene() = default;
    ~AdvanceGamePlayScene() override = default;

protected:
    // モード固有の初期化（スキル初期化、手札生成）
    void OnModeEnter(GameContext& gameContext) override;
    // モード固有の更新（凍結スキル判定、スキルカードクリック・AI思考）
    void OnModeUpdate(float dt, GameContext& gameContext) override;
    // 2D描画（スキルカード手札、ツールチップ、重複ダイス描画）
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;
    // ターゲット選択モード中（蘇生スキル発動時など）の判定
    bool IsModeSelectingTarget() const override;
    // ターゲット選択モード時のマス目クリックハンドラー
    void OnModeGridClicked(int gridX, int gridY) override;

private:
    SkillManager m_skillManager; // スキルカードおよびデッキの管理マネージャー
};