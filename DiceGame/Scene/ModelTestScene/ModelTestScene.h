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

// 3Dモデルの配置テストや挙動の検証を行うためのシーンクラス
// EditorSceneと同様に Imase::SceneBase を継承し、SceneManager経由で管理されます
class ModelTestScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    ModelTestScene() = default;
    ~ModelTestScene() override = default;

    // シーン切り替え時に一度だけ呼ばれる初期化処理を行います
    // カメラ、グリッド床、テスト用ダイスやJSONファイルからのステージオブジェクト読み込みを実行します
    void OnEnter(GameContext& gameContext) override;

    // 毎フレームの更新処理（入力検出、カメラ制御、ダイスの物理更新など）を行います
    // シーンマネージャーのUpdate関数から呼び出されます
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 毎フレームの描画処理（グリッド、配置オブジェクト、ダイス、コライダー表示など）を行います
    // シーンマネージャーのRender関数から呼び出されます
    void Render(GameContext& gameContext) override;

private:
    // JSON形式のステージデータから配置オブジェクト情報（PlacedObject）を読み込みます
    // OnEnter処理の内部から呼び出されます
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

private:
    // カメラ操作、グリッド表示、当たり判定可視化を行うライブラリオブジェクト群です
    std::unique_ptr<Imase::DebugCamera> m_debugCamera;
    std::unique_ptr<Imase::GridFloor> m_gridFloor;
    std::unique_ptr<Imase::CollisionRenderer> m_collisionRenderer;

    // 描画で使用するビュー行列およびプロジェクション行列を保持します
    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    // キーボードの1回押し（トラッキング）判定用オブジェクトです
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    // テスト動作確認用のダイスオブジェクトです
    Dice m_dice;

    // JSONから読み込んだ背景モデルなどの配置用オブジェクトリストです
    std::vector<PlacedObject> m_stageObjects;

    // 当たり判定（コライダー）のデバッグ表示フラグです
    bool m_showCollision = true;
};