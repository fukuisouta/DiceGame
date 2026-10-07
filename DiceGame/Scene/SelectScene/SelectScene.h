#pragma once

#include "../SceneId.h"
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "SceneModelManager/SceneModelManager.h"

#include <DirectXMath.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <Mouse.h>
#include <wrl/client.h>
#include <memory>
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
    void UpdateLayout(const RECT& outputSize);

private:
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    // JSON配置モデルマネージャー
    SceneModelManager m_sceneModels;

    DirectX::Mouse::ButtonStateTracker m_mouseTracker;

    RECT m_originalBtnRect{};
    RECT m_advanceBtnRect{};

    bool m_isOriginalHovered = false;
    bool m_isAdvanceHovered = false;
};