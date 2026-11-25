#pragma once

#include "../Base/GamePlaySceneBase.h"
#include "SkillManager.h"

namespace AdvanceSceneConstants
{
    constexpr float THINKING_TEXT_POS_X_OFFSET = 120.0f;
    constexpr float THINKING_TEXT_POS_Y = 20.0f;
    constexpr int REVIVE_GRID_Y_MIN = 6;
    constexpr int REVIVE_GRID_Y_MAX = 7;
    constexpr size_t CLICK_PARTICLE_COUNT = 15;
}

// アドバンスモード（スキルカード要素を追加したゲームモード）の派生シーン
class AdvanceGamePlayScene : public GamePlaySceneBase
{
public:
    AdvanceGamePlayScene() = default;
    ~AdvanceGamePlayScene() override = default;

    void OnModeEnter(GameContext& gameContext) override;
    void OnModeUpdate(float dt, GameContext& gameContext) override;
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;

    bool IsModeSelectingTarget() const override;
    void OnModeGridClicked(int gridX, int gridY) override;

private:
    bool IsMyTurn(GameContext& gameContext) const;

private:
    SkillManager m_skillManager;
    PlayerType m_myPlayerType = PlayerType::Player1;
    bool m_isClient = false;
};