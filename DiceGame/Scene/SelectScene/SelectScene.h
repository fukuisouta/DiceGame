#pragma once

#include "../SceneId.h"
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "Framework/ObjectData.h"

#include <DirectXMath.h>
#include <SimpleMath.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <WICTextureLoader.h>
#include <Mouse.h>
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

    // 2D/3D描画用 PrimitiveBatch
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;

    // 配置オブジェクト・テクスチャキャッシュ
    std::vector<PlacedObject> m_placedObjects;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

    DirectX::Mouse::ButtonStateTracker m_mouseTracker;

    // ボタン判定用 RECT & ホバーフラグ
    RECT m_originalBtnRect{};
    RECT m_advanceBtnRect{};

    bool m_isOriginalHovered = false;
    bool m_isAdvanceHovered = false;
};