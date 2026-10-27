#pragma once

#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <WICTextureLoader.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "Framework/ObjectData.h"

#include "Network/NetworkLobbyUI.h"
#include "Framework/SoundManager.h"
#include "ParticleManager.h" // ★ パーティクルマネージャーのインクルード

// ============================================================================
// タイトル画面シーンクラス
// シングル（1P vs CPU）、マルチ（オンライン対戦）、EXIT ボタンおよび背景演出の制御を行います
// ============================================================================
class TitleScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    TitleScene() = default;
    ~TitleScene() override = default;

    // シーン進入時の初期化処理（リソース生成、JSONレイアウトデータのロード）
    void OnEnter(GameContext& gameContext) override;
    // 毎フレームのロジック・入力検知・ボタンホバー判定・画面遷移処理
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    // 3D背景モデルおよび2D UI要素の描画処理
    void Render(GameContext& gameContext) override;

private:
    // テクスチャのロードおよびキャッシュ管理ヘルパー関数
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    // PrimitiveBatchを使用した2Dスプライト（板ポリゴン）描画処理
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& world);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    // 2D Canvas 描画用 PrimitiveBatch
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;

    std::vector<PlacedObject> m_placedObjects; // JSONより読み込んだ配置オブジェクトリスト
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_textureCache; // テクスチャキャッシュマップ

    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker; // キーボード入力追跡オブジェクト

    // 各ボタンのスクリーン座標判定用 RECT とホバー状態
    RECT m_soloBtnRect = {0, 0, 0, 0};
    RECT m_multiBtnRect = {0, 0, 0, 0};
    RECT m_exitBtnRect = {0, 0, 0, 0};

    bool m_isSoloHovered = false;  // 「ソロプレイ」ボタンホバーフラグ
    bool m_isMultiHovered = false; // 「マルチプレイ」ボタンホバーフラグ
    bool m_isExitHovered = false;  // 「ゲーム終了」ボタンホバーフラグ

    // マルチプレイ接続ロビーの表示制御フラグ
    bool m_showMultiLobby = false;

    NetworkLobbyUI m_networkUI;        // ネットワーク接続ロビーUI
    ParticleManager m_particleManager; // ★ パーティクルマネージャーのインスタンス
};