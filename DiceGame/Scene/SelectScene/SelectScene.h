#pragma once

#include "../SceneId.h"
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "Framework/ObjectData.h"
#include "GifTexture.h"

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

class SelectScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    SelectScene() = default;
    ~SelectScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;

    std::vector<PlacedObject> m_placedObjects;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

    DirectX::Mouse::ButtonStateTracker m_mouseTracker;

    RECT m_originalBtnRect{};
    RECT m_advanceBtnRect{};
    RECT m_ruleBtnRect{};

    // ★ 左右矢印ボタン用 RECT とホバー判定
    RECT m_prevArrowRect{};
    RECT m_nextArrowRect{};
    bool m_isPrevHovered = false;
    bool m_isNextHovered = false;

    bool m_isOriginalHovered = false;
    bool m_isAdvanceHovered = false;
    bool m_isRuleBtnHovered = false;

    // ── GIF アニメーション＆ページ管理 ──
    bool m_showRulePanel = false; // 最初はOFFにしてスッキリ表示
    int m_currentGuidePage = 0;   // 0: サイコロを振る, 1: コマの移動, 2: 相手を撃破

    GifTexture m_gifRoll;  // Roll.gif
    GifTexture m_gifMove;  // Move.gif
    GifTexture m_gifClash; // Kill.gif
};