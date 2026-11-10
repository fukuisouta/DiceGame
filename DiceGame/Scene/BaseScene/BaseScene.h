//--------------------------------------------------------------------------------------
// File: BaseScene.h
//
// 新規シーン作成時の元にするファイル
//
// Date: 2026.4.13
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"

// ベースシーンクラス定数定義
namespace BaseSceneConstants
{
    // 将来的な座標・カラー・タイマーパラメータ等の定数を定義するエリア
}

// すべてのゲームシーンの基礎となる基底シーンクラス
class BaseScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    BaseScene() = default;
    ~BaseScene() override = default;

    // フレームごとのシーン更新ロジックを実行する関数
    // 引数 sceneController: シーン遷移を管理するコントローラー参照, gameContext: ゲーム全体の共有コンテキスト
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // シーンの3D/2Dグラフィック描画処理を実行する関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void Render(GameContext& gameContext) override;

    // 他のシーンから本シーンへ切り替わった際に呼び出される初期化関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void OnEnter(GameContext& gameContext) override;
};