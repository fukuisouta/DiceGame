#pragma once
#include "GamePlaySceneBase.h"

class OriginalGamePlayScene : public GamePlaySceneBase
{
public:
    OriginalGamePlayScene() = default;
    ~OriginalGamePlayScene() override = default;

protected:
    void OnModeEnter(GameContext& gameContext) override;
    void OnModeUpdate(float dt, GameContext& gameContext) override;
    void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                        GameContext& gameContext) override;
    bool IsModeSelectingTarget() const override
    {
        return false;
    }
    void OnModeGridClicked(int gridX, int gridY) override
    {
    }
};