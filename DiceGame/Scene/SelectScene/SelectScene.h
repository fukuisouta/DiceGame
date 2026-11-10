#pragma once

#include "../SceneId.h"
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "Framework/ObjectData.h"
#include "GifTexture.h"
#include "Framework/SoundManager.h"
#include "ParticleManager.h"

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

// モード選択シーン定数定義
namespace SelectConstants
{
    constexpr float DEFAULT_FOV_DEGREE = 45.0f; // カメラ視野角 (度)
    constexpr float CAMERA_NEAR_Z = 0.1f;       // ニアクリップ平面
    constexpr float CAMERA_FAR_Z = 1000.0f;     // ファークリップ平面
    constexpr float ORTHO_HEIGHT = 10.0f;       // 正射影空間の基準高さ
    constexpr int TOTAL_GUIDE_PAGES = 3;        // ガイドページの総数
}

// ゲームモード選択画面（オリジナルモード／アドバンスモードおよびルール説明パネル）のシーンクラス
class SelectScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    SelectScene() = default;
    ~SelectScene() override = default;

    // シーン進入時の初期化処理（リソース読み込み、JSON配置データ復元）を行う関数
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void OnEnter(GameContext& gameContext) override;

    // 毎フレームの更新処理（ボタン判定、GIF更新、オンライン選択パケット同期）を行う関数
    // 引数 sceneController: シーンコントローラー参照, gameContext: 共有ゲームコンテキスト参照
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 3D背景・2D UI・ルール説明パネルのレンダリングを行う関数
    // 引数 gameContext: 共有ゲームコンテキスト参照
    void Render(GameContext& gameContext) override;

private:
    // テクスチャのキャッシュ読み込みを行うヘルパー関数
    // 引数 device: Direct3D11デバイス, texName: テクスチャファイル名
    // 戻り値: シェーダーリソースビューのComPtr
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);

    // PrimitiveBatchを使用した2D板ポリゴン（クワッド）描画関数
    // 引数 context: デバイスコンテキスト
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;     // エフェクトファクトリ
    std::unique_ptr<DirectX::BasicEffect> m_effect;          // ベーシックエフェクト
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout; // 入力レイアウト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>>
        m_primitiveBatch; // ポリゴン描画バッチ

    std::vector<PlacedObject> m_placedObjects; // 配置オブジェクト配列
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_textureCache; // テクスチャキャッシュマップ

    DirectX::Mouse::ButtonStateTracker m_mouseTracker; // マウストラッカー

    RECT m_originalBtnRect{}; // オリジナルモードボタン領域
    RECT m_advanceBtnRect{};  // アドバンスモードボタン領域
    RECT m_ruleBtnRect{};     // ルールボタン領域

    RECT m_prevArrowRect{};       // 前ページ矢印ボタン領域
    RECT m_nextArrowRect{};       // 次ページ矢印ボタン領域
    bool m_isPrevHovered = false; // 前ページ矢印ホバーフラグ
    bool m_isNextHovered = false; // 次ページ矢印ホバーフラグ

    bool m_isOriginalHovered = false; // オリジナルボタンホバーフラグ
    bool m_isAdvanceHovered = false;  // アドバンスボタンホバーフラグ
    bool m_isRuleBtnHovered = false;  // ルールボタンホバーフラグ

    bool m_showRulePanel = false; // ルールパネル表示フラグ
    int m_currentGuidePage = 0;   // ガイドページインデックス

    GifTexture m_gifRoll;  // サイコロ振りGIFテクスチャ
    GifTexture m_gifMove;  // コマ移動GIFテクスチャ
    GifTexture m_gifClash; // 撃破GIFテクスチャ

    ParticleManager m_particleManager; // パーティクルマネージャー
};