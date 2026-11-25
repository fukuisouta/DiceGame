#include "pch.h"
#include "AdvanceGamePlayScene.h"
#include "Network/NetworkPackets.h"
#include <Mouse.h>
#include <Keyboard.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace AdvanceStyleConstants
{
    constexpr float DUPLICATE_DICE_OFFSET_X = 1.8f;

    constexpr float CARD_WIDTH = 100.0f;
    constexpr float CARD_HEIGHT = 150.0f;
    constexpr float CARD_SPACING = 16.0f;
    constexpr float HAND_START_X = 20.0f;
    constexpr float HAND_MARGIN_BOTTOM = 20.0f;
    constexpr float HOVER_OFFSET_Y = 12.0f;

    constexpr float TOOLTIP_BG_WIDTH = 260.0f;
    constexpr float TOOLTIP_LINE_HEIGHT = 26.0f;
    constexpr float TOOLTIP_MARGIN_Y = 16.0f;
    constexpr float TOOLTIP_PADDING_X = 12.0f;
    constexpr float TOOLTIP_PADDING_Y = 10.0f;
    constexpr float TOOLTIP_TEXT_STEP_Y = 24.0f;
    constexpr float TOOLTIP_MIN_MARGIN = 10.0f;
    constexpr float TOOLTIP_OFFSET_Y = 8.0f;
    constexpr size_t DESCRIPTION_CHUNK_SIZE = 10;

    constexpr float DEBUG_TEXT_POS_X = 10.0f;
    constexpr float DEBUG_TEXT_POS_Y = 35.0f;
}

bool AdvanceGamePlayScene::IsMyTurn(GameContext& gameContext) const
{
    if (!gameContext.networkManager.IsConnected())
    {
        return true;
    }

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();
    uint8_t assignedRole = gameContext.networkManager.GetAssignedPlayerType();

    if (assignedRole == 1)
    {
        return (currentTurn == PlayerType::Player1);
    }
    else if (assignedRole == 2)
    {
        return (currentTurn == PlayerType::Player2);
    }

    return false;
}

void AdvanceGamePlayScene::OnModeEnter(GameContext& gameContext)
{
    m_gameLogic.SetAdvanceMode(true);
    m_skillManager.Initialize(gameContext.deviceResources.GetD3DDevice());
    m_skillManager.InitializeHandCards();

    if (gameContext.networkManager.IsConnected())
    {
        uint8_t assignedRole = gameContext.networkManager.GetAssignedPlayerType();
        m_isClient = (assignedRole == 2);
        m_myPlayerType = m_isClient ? PlayerType::Player2 : PlayerType::Player1;

        if (m_isClient)
        {
            AddLog(L"【2P(赤) / GUEST】として接続中（アドバンスモード）");
        }
        else
        {
            AddLog(L"【1P(青) / HOST】として接続中（アドバンスモード）");
        }
    }
    else
    {
        m_isClient = false;
        m_myPlayerType = PlayerType::Player1;
    }

    gameContext.networkManager.SetOnSkillUse(
        [this, &gameContext](uint8_t skillType, int targetX, int targetY)
        {
            SkillType st = static_cast<SkillType>(skillType);
            bool hasMoved = m_movementController.HasMovedThisTurn();
            PlayerType enemyPlayerType =
                (m_myPlayerType == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;

            m_skillManager.ProcessRemoteSkillClick(st, this, m_gameLogic, hasMoved, enemyPlayerType, targetX, targetY);

            // ホスト(1P)ならゲストのスキル反映後に盤面を同期
            if (gameContext.networkManager.GetAssignedPlayerType() == 1)
            {
                SendHostWorldSync(gameContext);
            }
        });
}

void AdvanceGamePlayScene::OnModeUpdate(float dt, GameContext& gameContext)
{
    if (gameContext.networkManager.IsConnected() && m_movementController.HasMovedThisTurn() &&
        m_gameLogic.GetRemainingSteps() <= 0 && !m_gameLogic.IsAnyPieceMoving())
    {
        if (gameContext.networkManager.GetAssignedPlayerType() == 1)
        {
            SendHostWorldSync(gameContext);
        }

        gameContext.networkManager.SendEndTurn();

        if (IsMyTurn(gameContext))
        {
            m_gameLogic.ForceEndTurn();
        }
        ResetMoveState();
    }

    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        m_movementController.Reset();
    }

    PlayerType currentTurn = m_gameLogic.GetCurrentTurn();

    if (m_gameLogic.GetRemainingSteps() <= 0)
    {
        m_skillManager.SetDoubleRollActive(false);
    }

    if (currentTurn == PlayerType::Player2 && !gameContext.networkManager.IsConnected())
    {
        bool hasMoved = m_movementController.HasMovedThisTurn();
        m_skillManager.ProcessAISkillLogic(dt, this, m_gameLogic, hasMoved);
    }

    if (IsMyTurn(gameContext) && !m_gameLogic.IsWaitingForRoll())
    {
        auto mouseState = DirectX::Mouse::Get().GetState();
        static DirectX::Mouse::ButtonStateTracker mouseTracker;
        mouseTracker.Update(mouseState);

        bool hasMoved = m_movementController.HasMovedThisTurn();
        auto& handCards = m_skillManager.GetHandCards();

        for (auto& card : handCards)
        {
            card.isHovered = (mouseState.x >= card.rect.left && mouseState.x <= card.rect.right &&
                              mouseState.y >= card.rect.top && mouseState.y <= card.rect.bottom);

            if (card.isHovered && !card.isUsed &&
                mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
            {
                if (card.type == SkillType::Revive && !m_gameLogic.HasDeadPiece(m_myPlayerType))
                    continue;

                m_particleManager.SpawnClickParticles(static_cast<float>(mouseState.x),
                                                      static_cast<float>(mouseState.y),
                                                      AdvanceSceneConstants::CLICK_PARTICLE_COUNT);

                m_skillManager.ProcessPlayerSkillClick(card, this, m_gameLogic, hasMoved, m_myPlayerType);

                if (gameContext.networkManager.IsConnected())
                {
                    gameContext.networkManager.SendSkillUse(static_cast<uint8_t>(card.type));
                    if (gameContext.networkManager.GetAssignedPlayerType() == 1)
                    {
                        SendHostWorldSync(gameContext);
                    }
                }
            }
        }
    }
}

bool AdvanceGamePlayScene::IsModeSelectingTarget() const
{
    return m_skillManager.IsReviveSelecting();
}

void AdvanceGamePlayScene::OnModeGridClicked(int gridX, int gridY)
{
    if (m_skillManager.IsReviveSelecting())
    {
        if (gridX >= 0 && gridX < DiceBattleGame::BOARD_WIDTH &&
            (gridY == AdvanceSceneConstants::REVIVE_GRID_Y_MIN || gridY == AdvanceSceneConstants::REVIVE_GRID_Y_MAX))
        {
            m_skillManager.ExecuteReviveSkill(gridX, gridY, this, m_gameLogic, m_myPlayerType);
        }
    }
}

void AdvanceGamePlayScene::OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                          GameContext& gameContext)
{
    auto& debugRenderer = gameContext.debugRenderer;

    if (gameContext.networkManager.IsConnected() && !IsMyTurn(gameContext))
    {
        debugRenderer.DrawText({screenWidth * 0.5f - AdvanceSceneConstants::THINKING_TEXT_POS_X_OFFSET,
                                AdvanceSceneConstants::THINKING_TEXT_POS_Y},
                               Vector4(1.0f, 0.8f, 0.2f, 1.0f), L"【あいての思考・操作中...】");
    }

    if (m_skillManager.IsReviveSelecting())
        debugRenderer.DrawText({AdvanceStyleConstants::DEBUG_TEXT_POS_X, AdvanceStyleConstants::DEBUG_TEXT_POS_Y},
                               L"下2行の空きマスを選択して駒を蘇生してください（ESCでキャンセル）");

    if (m_skillManager.IsDoubleRollActive() && m_gameLogic.GetRemainingSteps() > 0)
    {
        const auto* stepDice = m_gameLogic.GetStepDiceInstance();
        if (stepDice)
        {
            Matrix view =
                Matrix::CreateLookAt(m_cameraManager.GetEye(), m_cameraManager.GetTarget(), Vector3(0.0f, 1.0f, 0.0f));
            Matrix proj = Matrix::CreatePerspectiveFieldOfView(
                XMConvertToRadians(GamePlayBaseConstants::FOV_ANGLE_Y), screenWidth / screenHeight,
                GamePlayBaseConstants::NEAR_PLANE, GamePlayBaseConstants::FAR_PLANE);
            Matrix duplicateWorld =
                stepDice->GetWorldMatrix() *
                Matrix::CreateTranslation(AdvanceStyleConstants::DUPLICATE_DICE_OFFSET_X, 0.0f, 0.0f);
            stepDice->Render(context, gameContext.commonStates, duplicateWorld, view, proj);
        }
    }

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(Matrix::Identity);
    m_effect->SetProjection(orthoProj);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    float cardWidth = AdvanceStyleConstants::CARD_WIDTH;
    float cardHeight = AdvanceStyleConstants::CARD_HEIGHT;
    float cardSpacing = AdvanceStyleConstants::CARD_SPACING;
    float handStartX = AdvanceStyleConstants::HAND_START_X;
    float handStartY = screenHeight - cardHeight - AdvanceStyleConstants::HAND_MARGIN_BOTTOM;

    void const* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(
        VertexPositionTexture::InputElements, VertexPositionTexture::InputElementCount, shaderByteCode, byteCodeLength,
        inputLayout.GetAddressOf());

    context->IASetInputLayout(inputLayout.Get());
    auto& handCards = m_skillManager.GetHandCards();
    bool hasMoved = m_movementController.HasMovedThisTurn();

    for (size_t i = 0; i < handCards.size(); ++i)
    {
        auto& card = handCards[i];
        float cx = handStartX + i * (cardWidth + cardSpacing);
        float cy = handStartY;

        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(m_myPlayerType);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !hasMoved);

        if (card.isHovered && !card.isUsed && (!isReviveSkill || hasDead) && (!isReRollSkill || canReRoll))
            cy -= AdvanceStyleConstants::HOVER_OFFSET_Y;

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

    void const* colorByteCode = nullptr;
    size_t colorByteCodeLength = 0;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    gameContext.deviceResources.GetD3DDevice()->CreateInputLayout(VertexPositionColor::InputElements,
                                                                  VertexPositionColor::InputElementCount, colorByteCode,
                                                                  colorByteCodeLength, colorInputLayout.GetAddressOf());

    context->IASetInputLayout(colorInputLayout.Get());
    m_colorEffect->Apply(context);

    bool hasOverlayToDraw = false;
    for (size_t i = 0; i < handCards.size(); ++i)
    {
        const auto& card = handCards[i];
        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(m_myPlayerType);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !hasMoved);

        if (card.isUsed || (isReviveSkill && !hasDead) || (isReRollSkill && !canReRoll))
        {
            hasOverlayToDraw = true;
            break;
        }
    }

    if (hasOverlayToDraw)
    {
        m_colorPrimitiveBatch->Begin();
        for (size_t i = 0; i < handCards.size(); ++i)
        {
            const auto& card = handCards[i];
            float cx = static_cast<float>(card.rect.left), cy = static_cast<float>(card.rect.top);

            bool isReviveSkill = (card.type == SkillType::Revive);
            bool hasDead = m_gameLogic.HasDeadPiece(m_myPlayerType);
            bool isReRollSkill = (card.type == SkillType::ReRoll);
            bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !hasMoved);

            if (card.isUsed || (isReviveSkill && !hasDead) || (isReRollSkill && !canReRoll))
            {
                Vector4 overlayColor(0.0f, 0.0f, 0.0f, 0.65f);
                VertexPositionColor v0(Vector3(cx, cy, 0.0f), overlayColor);
                VertexPositionColor v1(Vector3(cx + cardWidth, cy, 0.0f), overlayColor);
                VertexPositionColor v2(Vector3(cx, cy + cardHeight, 0.0f), overlayColor);
                VertexPositionColor v3(Vector3(cx + cardWidth, cy + cardHeight, 0.0f), overlayColor);
                m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
                m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
            }
        }
        m_colorPrimitiveBatch->End();
    }

    for (size_t i = 0; i < handCards.size(); ++i)
    {
        const auto& card = handCards[i];
        bool isReviveSkill = (card.type == SkillType::Revive);
        bool hasDead = m_gameLogic.HasDeadPiece(m_myPlayerType);
        bool isReRollSkill = (card.type == SkillType::ReRoll);
        bool canReRoll = (m_gameLogic.GetRemainingSteps() > 0 && !hasMoved);

        if (card.isHovered && !card.isUsed && (!isReviveSkill || hasDead) && (!isReRollSkill || canReRoll))
        {
            std::vector<std::wstring> lines = {L"【" + card.name + L"】"};
            const size_t chunkSize = AdvanceStyleConstants::DESCRIPTION_CHUNK_SIZE;
            for (size_t p = 0; p < card.description.length(); p += chunkSize)
                lines.push_back(card.description.substr(p, chunkSize));

            float bgW = AdvanceStyleConstants::TOOLTIP_BG_WIDTH;
            float bgH = static_cast<float>(lines.size()) * AdvanceStyleConstants::TOOLTIP_LINE_HEIGHT +
                        AdvanceStyleConstants::TOOLTIP_MARGIN_Y;
            float tooltipX = std::clamp(static_cast<float>(card.rect.left), AdvanceStyleConstants::TOOLTIP_MIN_MARGIN,
                                        screenWidth - bgW);
            float tooltipY = static_cast<float>(card.rect.top) - bgH - AdvanceStyleConstants::TOOLTIP_OFFSET_Y;
            if (tooltipY < AdvanceStyleConstants::TOOLTIP_MIN_MARGIN)
                tooltipY = static_cast<float>(card.rect.bottom) + AdvanceStyleConstants::TOOLTIP_OFFSET_Y;

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
                debugRenderer.DrawText({tooltipX + AdvanceStyleConstants::TOOLTIP_PADDING_X,
                                        tooltipY + AdvanceStyleConstants::TOOLTIP_PADDING_Y +
                                            (l * AdvanceStyleConstants::TOOLTIP_TEXT_STEP_Y)},
                                       lines[l].c_str());
        }
    }

    m_effect->SetWorld(Matrix::Identity);
    m_effect->SetView(Matrix::Identity);
    m_effect->SetProjection(Matrix::Identity);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(Matrix::Identity);

    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
}