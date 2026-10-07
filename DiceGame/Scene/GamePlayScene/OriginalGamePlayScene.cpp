#include "pch.h"
#include "OriginalGamePlayScene.h"

void OriginalGamePlayScene::OnModeEnter(GameContext& gameContext)
{
    m_gameLogic.SetAdvanceMode(false);
}

void OriginalGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    // オリジナルモード特有の更新は特になし
}

//void OriginalGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
//                                           GameContext& gameContext)
//{
//    gameContext.debugRenderer.DrawText({screenWidth - 220.0f, 10.0f}, L"MODE: ORIGINAL");
//}
//  未使用引数はコメントアウトまたは UNREFERENCED_PARAMETER で処理
void OriginalGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float /*screenHeight*/,
                                           GameContext& gameContext)
    {}