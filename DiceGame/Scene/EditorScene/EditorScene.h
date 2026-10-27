#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <wrl/client.h>
#include <WICTextureLoader.h>

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "GridField.h"
#include "DiceBattleGame.h"
#include "Framework/ObjectData.h"

// ============================================================================
// レベルエディター/レイアウト編集シーンクラス
// 3Dモデルや2D UIスプライトの配置、レイアウトテスト、JSONシリアライズを担います
// ============================================================================
class EditorScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    EditorScene() = default;
    ~EditorScene() override = default;

    // シーン遷移時に呼び出される初期化処理
    void OnEnter(GameContext& gameContext) override;
    // 毎フレームのロジック・カメラ・UI入力更新
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    // 毎フレームの3D/2DオブジェクトおよびImGuiUIの描画処理
    void Render(GameContext& gameContext) override;

    // ファイルドラッグ＆ドロップイベントハンドラー（.cmo / .png / .json の自動読み込み）
    void OnFileDropped(GameContext& gameContext, const std::wstring& fullPath) override;

private:
    // JSONファイルからの配置オブジェクト読み込み
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
    // 現在の配置オブジェクトをJSONファイルへ保存
    void SaveSceneToJson(const std::string& filePath);

    // 視線レイ（Ray）とAABB境界ボックスの衝突判定（マウスクリック選択ピッキング用）
    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

    // テクスチャリソースのロードおよびキャッシュ取得
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    // 2D UI板ポリゴンの描画
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

    // オブジェクト操作用（コピー・ペースト・複製）
    void CopySelectedObject();
    void PasteObject();
    void DuplicateObject(int index);

private:
    GridField m_field;          // グリッド盤面表示
    DiceBattleGame m_gameLogic; // ダイスバトルロジック参照

    int m_editorMode = 1;                 // 0: Grid Mode（盤面編集）, 1: Scene Objects Mode（オブジェクト配置編集）
    bool m_isPlaying = false;             // エディター内プレビューテスト再生フラグ
    bool m_transitionToPlayScene = false; // 本編プレイシーンへの遷移要求フラグ

    // 現在編集対象となっているJSONファイルのパス
    std::string m_currentFilePath = "Resources/Data/title_scene.json";

    // シーン上に配置されている全オブジェクトの配列
    std::vector<PlacedObject> m_placedObjects;
    int m_selectedIndex = -1; // 現在選択中のオブジェクトインデックス

    // クリップボード用データ
    PlacedObject m_clipboardObject;
    bool m_hasClipboard = false;

    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f};

    // フリーカメラパラメータ
    DirectX::SimpleMath::Vector3 m_cameraPos = {0.0f, 0.0f, -10.0f};
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.0f;
    float m_cameraMoveSpeed = 15.0f;
    float m_cameraRotateSpeed = 0.005f;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;

    // 2D板ポリゴンおよびテクスチャ描画用リソース
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

    // プレビューテスト時のボタン領域およびホバー状態判定
    RECT m_startBtnRect = {0, 0, 0, 0};
    RECT m_exitBtnRect = {0, 0, 0, 0};
    bool m_isStartHovered = false;
    bool m_isExitHovered = false;
};