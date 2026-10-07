#pragma once

#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "SceneModelManager/SceneModelManager.h"

class TitleScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    TitleScene() = default;
    ~TitleScene() = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;

    // JSON配置モデルマネージャー
    SceneModelManager m_sceneModels;

    // ボタンの描画領域とホバー状態
    RECT m_startBtnRect = {0, 0, 0, 0};
    RECT m_exitBtnRect = {0, 0, 0, 0};
    bool m_isStartHovered = false;
    bool m_isExitHovered = false;
};