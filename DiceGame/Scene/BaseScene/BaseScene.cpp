//--------------------------------------------------------------------------------------
// File: BaseScene.cpp
//
// 新規シーン作成時の元にするファイル
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "BaseScene.h"

using namespace DirectX;

// デバッグ表示位置等の定数定義
namespace BaseSceneStyleConstants
{
    constexpr float DEBUG_TEXT_POS_X = 0.0f; // デバッグテキスト表示のX座標
    constexpr float DEBUG_TEXT_POS_Y = 0.0f; // デバッグテキスト表示のY座標
}

// フレーム更新処理
void BaseScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    // 未使用パラメータ警告防止
    (void)sceneController;

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;

    // 画面にデバッグテキストを描画
    debugRenderer.DrawText({BaseSceneStyleConstants::DEBUG_TEXT_POS_X, BaseSceneStyleConstants::DEBUG_TEXT_POS_Y},
                           L"BaseScene");
}

// 描画処理
void BaseScene::Render(GameContext& gameContext)
{
    // 未使用パラメータの明示
    (void)gameContext;
}

// シーン進入時（切り替え時）処理
void BaseScene::OnEnter(GameContext& gameContext)
{
    // 未使用パラメータの明示
    (void)gameContext;
}