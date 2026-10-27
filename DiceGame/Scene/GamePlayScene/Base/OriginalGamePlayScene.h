#pragma once

#include "GamePlaySceneBase.h"

// ============================================================================
// オリジナルモード（ネットワーク同期・クラシックルール）のゲームプレイシーン
// ============================================================================
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

private:
    // 自身が現在操作権利（手番）を持っているかを判定するヘルパー
    bool IsMyTurn(GameContext& gameContext) const;
};