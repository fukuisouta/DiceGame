#pragma once

#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <WICTextureLoader.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "Framework/ObjectData.h"

#include "Network/NetworkLobbyUI.h"

class TitleScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    TitleScene() = default;
    ~TitleScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& world);

private:
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    // 2D Canvas 描画用 PrimitiveBatch
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>> m_primitiveBatch;

    std::vector<PlacedObject> m_placedObjects;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    RECT m_startBtnRect = {0, 0, 0, 0};
    RECT m_exitBtnRect = {0, 0, 0, 0};
    bool m_isStartHovered = false;
    bool m_isExitHovered = false;

    NetworkLobbyUI m_networkUI;
};