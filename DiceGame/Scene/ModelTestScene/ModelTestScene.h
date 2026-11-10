#pragma once

#include <memory>
#include <vector>
#include <string>
#include "ImaseLib/SceneManager.h"
#include "ImaseLib/DebugCamera.h"
#include "ImaseLib/GridFloor.h"
#include "ImaseLib/CollisionRenderer.h"
#include "../SceneId.h"
#include "../Framework/ObjectData.h"
#include "Game.h"
#include "Dice/Dice.h"

// 3Dモデルテストシーン定数定義
namespace ModelTestConstants
{
    constexpr float DEFAULT_FOV_DEGREE = 45.0f; // カメラ画角（度）
    constexpr float NEAR_Z = 0.1f;              // ニアクリップ平面
    constexpr float FAR_Z = 1000.0f;            // ファークリップ平面
}

// 3Dモデルの配置テストや挙動の検証を行うためのシーンクラス
// SceneManager経由で管理されます
class ModelTestScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    ModelTestScene() = default;
    ~ModelTestScene() override = default;

    // シーン切り替え時に一度だけ呼ばれる初期化処理を行う関数
    // カメラ、グリッド床、テスト用ダイスやJSONファイルからのステージオブジェクト読み込みを実行します
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void OnEnter(GameContext& gameContext) override;

    // 毎フレームの更新処理（入力検出、カメラ制御、ダイスの物理更新など）を行う関数
    // 引数 sceneController: シーンコントローラー参照, gameContext: 共有ゲームコンテキスト参照
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 毎フレームの描画処理（グリッド、配置オブジェクト、ダイス、コライダー表示など）を行う関数
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void Render(GameContext& gameContext) override;

private:
    // JSON形式のステージデータから配置オブジェクト情報を読み込む関数
    // 引数 gameContext: 共有ゲームコンテキスト参照, filePath: 読み込み用JSONファイルパス
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

private:
    std::unique_ptr<Imase::DebugCamera> m_debugCamera;             // デバッグカメラ制御インスタンス
    std::unique_ptr<Imase::GridFloor> m_gridFloor;                 // グリッド床面描画インスタンス
    std::unique_ptr<Imase::CollisionRenderer> m_collisionRenderer; // コライダー可視化描画インスタンス

    DirectX::SimpleMath::Matrix m_view;       // ビュー行列
    DirectX::SimpleMath::Matrix m_projection; // プロジェクション行列

    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker; // キーボードトラッカー

    Dice m_dice; // テスト用ダイスオブジェクト

    std::vector<PlacedObject> m_stageObjects; // 背景モデル等の配置オブジェクト配列

    bool m_showCollision = true; // コライダー枠線表示フラグ
};