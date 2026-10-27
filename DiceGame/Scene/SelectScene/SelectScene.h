#pragma once

#include "../SceneId.h"
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "Framework/ObjectData.h"
#include "GifTexture.h"
#include "Framework/SoundManager.h"
#include "ParticleManager.h" // ★ パーティクルマネージャーのインクルード

#include <DirectXMath.h>
#include <SimpleMath.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <WICTextureLoader.h>
#include <Mouse.h>
#include <Keyboard.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <Windows.h>

// ============================================================================
// ゲームモード選択画面（オリジナルモード／アドバンスモードおよびルール説明パネル）
// ============================================================================
class SelectScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    SelectScene() = default;
    ~SelectScene() override = default;

    // シーン進入時の初期化処理（リソース読み込み、JSON配置データ復元）
    void OnEnter(GameContext& gameContext) override;
    // 毎フレームの更新処理（ボタン判定、GIF更新、オンライン選択パケット同期）
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    // 3D背景・2D UI・ルール説明パネルのレンダリング
    void Render(GameContext& gameContext) override;

private:
    // テクスチャのキャッシュ読み込みヘルパー
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    // PrimitiveBatchを使用した2D板ポリゴン（クワッド）描画関数
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;

    std::vector<PlacedObject> m_placedObjects; // JSONから読み込んだ配置オブジェクト一覧
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_textureCache; // テクスチャキャッシュ

    DirectX::Mouse::ButtonStateTracker m_mouseTracker; // マウスクリック判定トラッカー

    // ボタンの画面上矩形領域（クリック判定用）
    RECT m_originalBtnRect{};
    RECT m_advanceBtnRect{};
    RECT m_ruleBtnRect{};

    // 左右矢印ボタン用 RECT とホバー判定
    RECT m_prevArrowRect{};
    RECT m_nextArrowRect{};
    bool m_isPrevHovered = false;
    bool m_isNextHovered = false;

    // モード選択・ルールボタンホバーフラグ
    bool m_isOriginalHovered = false;
    bool m_isAdvanceHovered = false;
    bool m_isRuleBtnHovered = false;

    // ── GIF アニメーション＆ページ管理 ──
    bool m_showRulePanel = false; // ルールパネルの表示ON/OFFフラグ
    int m_currentGuidePage = 0;   // ガイドページインデックス（0: サイコロを振る, 1: コマの移動, 2: 相手を撃破）

    GifTexture m_gifRoll;  // Roll.gif アニメーションテクスチャ
    GifTexture m_gifMove;  // Move.gif アニメーションテクスチャ
    GifTexture m_gifClash; // Kill.gif アニメーションテクスチャ

    ParticleManager m_particleManager; // ★ パーティクルマネージャーのインスタンス
};