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
#include "ParticleManager.h"

// タイトルシーン定数定義
namespace TitleConstants
{
    constexpr float DEFAULT_FOV_DEGREE = 45.0f; // カメラ視野角 (度)
    constexpr float CAMERA_NEAR_Z = 0.1f;       // ニアクリップ平面
    constexpr float CAMERA_FAR_Z = 1000.0f;     // ファークリップ平面
    constexpr float ORTHO_HEIGHT = 10.0f;       // 正射影空間の基準高さ
}

// タイトル画面シーンクラス
// シングル（1P vs CPU）、マルチ（オンライン対戦）、EXIT ボタンおよび背景演出の制御を行うクラス
class TitleScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    TitleScene() = default;
    ~TitleScene() override = default;

    // シーン進入時の初期化処理（リソース生成、JSONレイアウトデータのロード）を行う関数
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void OnEnter(GameContext& gameContext) override;

    // 毎フレームのロジック・入力検知・ボタンホバー判定・画面遷移処理を行う関数
    // 引数 sceneController: シーンコントローラー参照, gameContext: 共有ゲームコンテキスト参照
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 3D背景モデルおよび2D UI要素の描画処理を行う関数
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void Render(GameContext& gameContext) override;

private:
    // テクスチャのロードおよびキャッシュ管理を行うヘルパー関数
    // 引数 device: Direct3D11デバイス, texName: テクスチャファイル名
    // 戻り値: シェーダーリソースビューのComPtr
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);

    // PrimitiveBatchを使用した2Dスプライト（板ポリゴン）描画処理を行う関数
    // 引数 context: デバイスコンテキスト, world: ワールド行列
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& world);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;     // エフェクトファクトリ
    std::unique_ptr<DirectX::BasicEffect> m_effect;          // ベーシックエフェクト
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout; // 入力レイアウト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>>
        m_primitiveBatch; // 2Dポリゴン描画バッチ

    std::vector<PlacedObject> m_placedObjects; // 配置オブジェクトリスト
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_textureCache; // テクスチャキャッシュマップ

    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker; // キーボードトラッカー

    RECT m_soloBtnRect = {0, 0, 0, 0};  // ソロボタン領域
    RECT m_multiBtnRect = {0, 0, 0, 0}; // マルチボタン領域
    RECT m_exitBtnRect = {0, 0, 0, 0};  // 終了ボタン領域

    bool m_isSoloHovered = false;  // ソロボタンホバーフラグ
    bool m_isMultiHovered = false; // マルチボタンホバーフラグ
    bool m_isExitHovered = false;  // 終了ボタンホバーフラグ

    bool m_showMultiLobby = false; // マルチロビー表示フラグ

    NetworkLobbyUI m_networkUI;        // ネットワーク接続ロビーUI
    ParticleManager m_particleManager; // パーティクルマネージャー
};