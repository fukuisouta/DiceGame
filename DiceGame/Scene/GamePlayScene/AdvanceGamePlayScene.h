#pragma once
#include "GamePlaySceneBase.h"
#include "SkillManager.h"

class AdvanceGamePlayScene : public GamePlaySceneBase
{
public:
    AdvanceGamePlayScene() = default;
    ~AdvanceGamePlayScene() override = default;

protected:
    void OnModeEnter(GameContext& gameContext) override;
    void OnModeUpdate(float dt, GameContext& gameContext) override;
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;
    bool IsModeSelectingTarget() const override;
    void OnModeGridClicked(int gridX, int gridY) override;

private:
    SkillManager m_skillManager;
};