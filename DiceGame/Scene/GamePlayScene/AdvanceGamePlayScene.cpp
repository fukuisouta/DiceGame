#include "pch.h"
#include "AdvanceGamePlayScene.h"
#include <Mouse.h>
#include <Keyboard.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

void AdvanceGamePlayScene::OnModeEnter(GameContext& gameContext)
{
    m_gameLogic.SetAdvanceMode(true);
    m_skillManager.Initialize(gameContext.deviceResources.GetD3DDevice());
    m_skillManager.InitializeHandCards();
}

void AdvanceGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    // 凍結スキルのチェック
    if (m_gameLogic.GetCurrentTurn() == PlayerType::Player2 && m_skillManager.IsFreezeNextTurn())
    {
        m_skillManager.SetFreezeNextTurn(false);
        m_gameLogic.SetRemainingSteps(0);
        m_skillManager.SetDoubleRollActive(false);
        m_gameLogic.ForceEndTurn();
        TriggerTurnNotice(L"相手は凍結中！", 1.5f);
    }

    // 残り歩数が尽きた場合ダブルロール終了
    if (m_gameLogic.GetRemainingSteps() <= 0)
    {
        m_skillManager.SetDoubleRollActive(false);
    }

    // CPUのスキル思考
    if (m_gameLogic.GetCurrentTurn() == PlayerType::Player2)
    {
        m_skillManager.ProcessAISkillLogic(dt, this, m_gameLogic, m_hasMovedThisTurn);
    }

    // プレイヤーのスキルカードクリック処理
    if (m_gameLogic.GetCurrentTurn() == PlayerType::Player1 && !m_gameLogic.IsWaitingForRoll())
    {
        auto mouseState = DirectX::Mouse::Get().GetState();
        static DirectX::Mouse::ButtonStateTracker mouseTracker;
        mouseTracker.Update(mouseState);

        auto& handCards = m_skillManager.GetHandCards();
        for (auto& card : handCards)
        {
            card.isHovered = (mouseState.x >= card.rect.left && mouseState.x <= card.rect.right &&
                              mouseState.y >= card.rect.top && mouseState.y <= card.rect.bottom);

            if (card.isHovered && !card.isUsed &&
                mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
            {
                if (card.type == SkillType::Revive && !m_gameLogic.HasDeadPiece(PlayerType::Player1))
                    continue;
                if (card.type == SkillType::ReRoll && (m_gameLogic.GetRemainingSteps() == 0 || m_hasMovedThisTurn))
                    continue;

                m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x),
                                                      static_cast<float>(mouseState.y), 45);
                m_skillManager.ProcessPlayerSkillClick(card, this, m_gameLogic, m_hasMovedThisTurn);
            }
        }
    }
}

bool AdvanceGamePlayScene::IsModeSelectingTarget() const
{
    return m_skillManager.IsDestroySelecting() || m_skillManager.IsReviveSelecting();
}

void AdvanceGamePlayScene::OnModeGridClicked(int gridX, int gridY)
{
    if (m_skillManager.IsDestroySelecting())
    {
        if (gridX >= 0 && gridX < DiceBattleGame::BOARD_WIDTH && gridY >= 0 && gridY < DiceBattleGame::BOARD_HEIGHT)
            m_skillManager.ExecuteDestroySkill(gridX, gridY, this, m_gameLogic);
    }
    else if (m_skillManager.IsReviveSelecting())
    {
        if (gridX >= 0 && gridX < DiceBattleGame::BOARD_WIDTH && (gridY == 6 || gridY == 7))
            m_skillManager.ExecuteReviveSkill(gridX, gridY, this, m_gameLogic);
    }
}

void AdvanceGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                          GameContext& gameContext)
{
    auto& debugRenderer = gameContext.debugRenderer;
    debugRenderer.DrawText({screenWidth - 210.0f, 10.0f}, L"MODE: ADVANCE");

    if (m_skillManager.IsDestroySelecting())
        debugRenderer.DrawText({10.0f, 35.0f}, L"破壊する相手の駒を選択してください（ESCでキャンセル）");
    else if (m_skillManager.IsReviveSelecting())
        debugRenderer.DrawText({10.0f, 35.0f}, L"下2行の空きマスを選択して駒を蘇生してください（ESCでキャンセル）");

    // 重複ダイス描画（2倍移動）
    if (m_skillManager.IsDoubleRollActive() && m_gameLogic.GetRemainingSteps() > 0)
    {
        const auto* stepDice = m_gameLogic.GetStepDiceInstance();
        if (stepDice)
        {
            Matrix view = Matrix::CreateLookAt(m_cameraEye, m_cameraTarget, Vector3(0.0f, 1.0f, 0.0f));
            Matrix proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45.0f), screenWidth / screenHeight,
                                                               0.1f, 1000.0f);
            Matrix duplicateWorld = stepDice->GetWorldMatrix() * Matrix::CreateTranslation(1.8f, 0.0f, 0.0f);
            stepDice->Render(context, gameContext.commonStates, duplicateWorld, view, proj);
        }
    }

    // スキルカード描画
    float cardWidth = 100.0f, cardHeight = 150.0f, cardSpacing = 16.0f;
    float handStartX = 20.0f, handStartY = screenHeight - cardHeight - 20.0f;

    void const* shaderByteCode;
    size_t byteCodeLength;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(
        VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount, shaderByteCode, byteCodeLength,
        inputLayout.GetAddressOf());

    context->IASetInputLayout(inputLayout.Get());
    auto& handCards = m_skillManager.GetHandCards();

    for (size_t i = 0; i < handCards.size(); ++i)
    {
        auto& card = handCards[i];
        float cx = handStartX + i * (cardWidth + cardSpacing);
        float cy = handStartY;

        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(PlayerType::Player1);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !m_hasMovedThisTurn);

        if (card.isHovered && !card.isUsed && (!isReviveSkill || hasDead) && (!isReRollSkill || canReRoll))
            cy -= 12.0f;

        card.rect = {static_cast<LONG>(cx), static_cast<LONG>(cy), static_cast<LONG>(cx + cardWidth),
                     static_cast<LONG>(cy + cardHeight)};

        if (card.texture)
        {
            m_effect->SetTexture(card.texture.Get());
            m_effect->Apply(context);
            m_primitiveBatch->Begin();
            VertexPositionTexture v0(Vector3(cx, cy, 0.0f), Vector2(0.0f, 0.0f));
            VertexPositionTexture v1(Vector3(cx + cardWidth, cy, 0.0f), Vector2(1.0f, 0.0f));
            VertexPositionTexture v2(Vector3(cx, cy + cardHeight, 0.0f), Vector2(0.0f, 1.0f));
            VertexPositionTexture v3(Vector3(cx + cardWidth, cy + cardHeight, 0.0f), Vector2(1.0f, 1.0f));
            m_primitiveBatch->DrawTriangle(v0, v1, v2);
            m_primitiveBatch->DrawTriangle(v2, v1, v3);
            m_primitiveBatch->End();
        }
    }

    // カードのグレーアウト＆ハイライト重ね描き
    void const* colorByteCode;
    size_t colorByteCodeLength;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(VertexPositionColor::InputElements,
                                                                  VertexPositionColor::InputElementCount, colorByteCode,
                                                                  colorByteCodeLength, colorInputLayout.GetAddressOf());

    context->IASetInputLayout(colorInputLayout.Get());
    m_colorEffect->Apply(context);
    m_colorPrimitiveBatch->Begin();

    for (size_t i = 0; i < handCards.size(); ++i)
    {
        const auto& card = handCards[i];
        float cx = static_cast<float>(card.rect.left), cy = static_cast<float>(card.rect.top);

        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(PlayerType::Player1);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !m_hasMovedThisTurn);

        Vector4 overlayColor(0, 0, 0, 0);
        if (card.isUsed || (isReviveSkill && !hasDead) || (isReRollSkill && !canReRoll))
            overlayColor = Vector4(0.0f, 0.0f, 0.0f, 0.65f);
        else if ((isReviveSkill && m_skillManager.IsReviveSelecting()) ||
                 (card.type == SkillType::Destroy && m_skillManager.IsDestroySelecting()))
            overlayColor = Vector4(1.0f, 0.2f, 1.0f, 0.3f);

        if (overlayColor.w > 0.0f)
        {
            VertexPositionColor v0(Vector3(cx, cy, 0), overlayColor);
            VertexPositionColor v1(Vector3(cx + cardWidth, cy, 0), overlayColor);
            VertexPositionColor v2(Vector3(cx, cy + cardHeight, 0), overlayColor);
            VertexPositionColor v3(Vector3(cx + cardWidth, cy + cardHeight, 0), overlayColor);
            m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
            m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
        }
    }
    m_colorPrimitiveBatch->End();

    // ツールチップ表示
    for (size_t i = 0; i < handCards.size(); ++i)
    {
        const auto& card = handCards[i];
        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(PlayerType::Player1);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !m_hasMovedThisTurn);

        if (card.isHovered && !card.isUsed && (!isReviveSkill || hasDead) && (!isReRollSkill || canReRoll))
        {
            std::vector<std::wstring> lines = {L"【" + card.name + L"】"};
            const size_t chunkSize = 10;
            for (size_t p = 0; p < card.description.length(); p += chunkSize)
                lines.push_back(card.description.substr(p, chunkSize));

            float bgW = 260.0f, bgH = static_cast<float>(lines.size()) * 26.0f + 16.0f;
            float tooltipX = std::clamp(static_cast<float>(card.rect.left), 10.0f, 1200.0f - bgW);
            float tooltipY = static_cast<float>(card.rect.top) - bgH - 8.0f;
            if (tooltipY < 10.0f)
                tooltipY = static_cast<float>(card.rect.bottom) + 8.0f;

            m_colorEffect->Apply(context);
            m_colorPrimitiveBatch->Begin();
            Vector4 bgCol(0.05f, 0.05f, 0.05f, 0.90f);
            VertexPositionColor bgV0(Vector3(tooltipX, tooltipY, 0.0f), bgCol);
            VertexPositionColor bgV1(Vector3(tooltipX + bgW, tooltipY, 0.0f), bgCol);
            VertexPositionColor bgV2(Vector3(tooltipX, tooltipY + bgH, 0.0f), bgCol);
            VertexPositionColor bgV3(Vector3(tooltipX + bgW, tooltipY + bgH, 0.0f), bgCol);
            m_colorPrimitiveBatch->DrawTriangle(bgV0, bgV1, bgV2);
            m_colorPrimitiveBatch->DrawTriangle(bgV2, bgV1, bgV3);
            m_colorPrimitiveBatch->End();

            for (size_t l = 0; l < lines.size(); ++l)
                debugRenderer.DrawText({tooltipX + 12.0f, tooltipY + 10.0f + (l * 24.0f)}, lines[l].c_str());
        }
    }
}