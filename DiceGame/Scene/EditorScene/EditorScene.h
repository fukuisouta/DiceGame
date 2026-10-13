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
#include "../GamePlayScene/GridField.h"
#include "../GamePlayScene/DiceBattleGame.h"
#include "Framework/ObjectData.h"

class EditorScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    EditorScene() = default;
    ~EditorScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

    void OnFileDropped(GameContext& gameContext, const std::wstring& fullPath) override;

private:
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
    void SaveSceneToJson(const std::string& filePath);

    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

    // コピー＆ペースト・複製用ヘルパー
    void CopySelectedObject();
    void PasteObject();
    void DuplicateObject(int index);

private:
    GridField m_field;
    DiceBattleGame m_gameLogic;

    int m_editorMode = 1;     // 0: Grid Mode, 1: Scene Objects Mode
    bool m_isPlaying = false; // エディター内プレビュー再生フラグ
    bool m_transitionToPlayScene = false;

    // 現在編集中のJSONファイルパス
    std::string m_currentFilePath = "Resources/Data/title_scene.json";

    // 配置オブジェクトデータ
    std::vector<PlacedObject> m_placedObjects;
    int m_selectedIndex = -1;

    // コピー＆ペースト用バッファ
    PlacedObject m_clipboardObject;
    bool m_hasClipboard = false;

    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f};
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f};

    // カメラパラメータ
    DirectX::SimpleMath::Vector3 m_cameraPos = {0.0f, 0.0f, -10.0f};
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.0f;
    float m_cameraMoveSpeed = 15.0f;
    float m_cameraRotateSpeed = 0.005f;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;

    // テクスチャ・板ポリゴン描画用リソース
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

    // プレビューテスト用ボタン領域・状態
    RECT m_startBtnRect = {0, 0, 0, 0};
    RECT m_exitBtnRect = {0, 0, 0, 0};
    bool m_isStartHovered = false;
    bool m_isExitHovered = false;
};