#include "pch.h"
#include "GameJudgeManager.h"
#include "ImaseLib/DebugRenderer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 初期化処理
void GameJudgeManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);

    m_resultState = ResultState::None;
    m_selectIndex = 0;
}

// 勝敗判定評価
void GameJudgeManager::JudgeGame(const DiceBattleGame& gameLogic, PlayerType myPlayerType)
{
    if (!gameLogic.IsGameOver())
    {
        m_resultState = ResultState::None;
        return;
    }

    if (gameLogic.IsDraw())
    {
        m_resultState = ResultState::Draw;
    }
    else if (gameLogic.GetWinner() == myPlayerType)
    {
        m_resultState = ResultState::Win;
    }
    else
    {
        m_resultState = ResultState::Lose;
    }
}

// 勝敗ダイアログ上のボタン選択・決定操作更新
GameJudgeManager::Action GameJudgeManager::Update(const Mouse::State& mouseState,
                                                  const Mouse::ButtonStateTracker& mouseTracker,
                                                  const Keyboard::KeyboardStateTracker& kbTracker)
{
    if (!IsGameOver())
        return Action::None;

    m_isRetryHovered = (mouseState.x >= m_retryButtonRect.left && mouseState.x <= m_retryButtonRect.right &&
                        mouseState.y >= m_retryButtonRect.top && mouseState.y <= m_retryButtonRect.bottom);
    m_isTitleHovered = (mouseState.x >= m_titleButtonRect.left && mouseState.x <= m_titleButtonRect.right &&
                        mouseState.y >= m_titleButtonRect.top && mouseState.y <= m_titleButtonRect.bottom);

    if (m_isRetryHovered)
        m_selectIndex = 0;
    if (m_isTitleHovered)
        m_selectIndex = 1;

    if (kbTracker.IsKeyPressed(Keyboard::Keys::Up) || kbTracker.IsKeyPressed(Keyboard::Keys::W))
        m_selectIndex = 0;
    if (kbTracker.IsKeyPressed(Keyboard::Keys::Down) || kbTracker.IsKeyPressed(Keyboard::Keys::S))
        m_selectIndex = 1;

    bool isClicked =
        (mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED) && (m_isRetryHovered || m_isTitleHovered);
    bool isEnterPressed =
        kbTracker.IsKeyPressed(Keyboard::Keys::Enter) || kbTracker.IsKeyPressed(Keyboard::Keys::Space);

    if (isClicked || isEnterPressed)
    {
        return (m_selectIndex == 0) ? Action::Retry : Action::GoToTitle;
    }

    return Action::None;
}

// 勝敗オーバーレイ・ダイアログ・選択ボタン・テキストの描画
void GameJudgeManager::Render(ID3D11DeviceContext* context, const CommonStates& states,
                              Imase::DebugRenderer& debugRenderer, float screenWidth, float screenHeight)
{
    if (!IsGameOver())
        return;

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    void const* colorByteCode = nullptr;
    size_t colorByteCodeLength = 0;
    m_colorEffect->GetVertexShaderBytecode(&colorByteCode, &colorByteCodeLength);
    Microsoft::WRL::ComPtr<ID3D11InputLayout> colorInputLayout;
    device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount, colorByteCode,
                              colorByteCodeLength, colorInputLayout.GetAddressOf());

    context->IASetInputLayout(colorInputLayout.Get());
    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.DepthNone(), 0);
    context->RSSetState(states.CullNone());

    m_colorEffect->Apply(context);
    m_colorPrimitiveBatch->Begin();

    // 画面全体の半透明暗転背景描画
    VertexPositionColor bg0(Vector3(0.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
    VertexPositionColor bg1(Vector3(screenWidth, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
    VertexPositionColor bg2(Vector3(0.0f, screenHeight, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
    VertexPositionColor bg3(Vector3(screenWidth, screenHeight, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.75f));
    m_colorPrimitiveBatch->DrawTriangle(bg0, bg1, bg2);
    m_colorPrimitiveBatch->DrawTriangle(bg2, bg1, bg3);

    // ポップアップウィンドウ本体の描画
    float dialogW = 400.0f, dialogH = 260.0f;
    float dialogX = (screenWidth - dialogW) * 0.5f;
    float dialogY = (screenHeight - dialogH) * 0.5f;

    VertexPositionColor dlg0(Vector3(dialogX, dialogY, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
    VertexPositionColor dlg1(Vector3(dialogX + dialogW, dialogY, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
    VertexPositionColor dlg2(Vector3(dialogX, dialogY + dialogH, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
    VertexPositionColor dlg3(Vector3(dialogX + dialogW, dialogY + dialogH, 0.0f), Vector4(0.12f, 0.12f, 0.18f, 0.95f));
    m_colorPrimitiveBatch->DrawTriangle(dlg0, dlg1, dlg2);
    m_colorPrimitiveBatch->DrawTriangle(dlg2, dlg1, dlg3);

    // リトライ / タイトル選択ボタンの描画
    float btnW = 220.0f, btnH = 45.0f;
    float btnX = dialogX + (dialogW - btnW) * 0.5f;
    float btnY1 = dialogY + 110.0f, btnY2 = dialogY + 175.0f;

    m_retryButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY1), static_cast<LONG>(btnX + btnW),
                         static_cast<LONG>(btnY1 + btnH)};
    m_titleButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY2), static_cast<LONG>(btnX + btnW),
                         static_cast<LONG>(btnY2 + btnH)};

    Vector4 col1 = (m_selectIndex == 0) ? Vector4(0.9f, 0.7f, 0.1f, 0.9f) : Vector4(0.3f, 0.35f, 0.45f, 0.8f);
    Vector4 col2 = (m_selectIndex == 1) ? Vector4(0.9f, 0.7f, 0.1f, 0.9f) : Vector4(0.3f, 0.35f, 0.45f, 0.8f);

    auto DrawBox = [&](float x, float y, float w, float h, Vector4 col)
    {
        VertexPositionColor v0(Vector3(x, y, 0.0f), col);
        VertexPositionColor v1(Vector3(x + w, y, 0.0f), col);
        VertexPositionColor v2(Vector3(x, y + h, 0.0f), col);
        VertexPositionColor v3(Vector3(x + w, y + h, 0.0f), col);
        m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
        m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
    };

    DrawBox(btnX, btnY1, btnW, btnH, col1);
    DrawBox(btnX, btnY2, btnW, btnH, col2);
    m_colorPrimitiveBatch->End();

    // 勝敗テキストの設定
    const wchar_t* resultStr = L"";
    float textOffsetX = 130.0f;

    switch (m_resultState)
    {
        case ResultState::Win:
            resultStr = L"YOU WIN";
            textOffsetX = 135.0f;
            break;
        case ResultState::Lose:
            resultStr = L"YOU LOSE...";
            textOffsetX = 125.0f;
            break;
        case ResultState::Draw:
            resultStr = L"DRAW";
            textOffsetX = 155.0f;
            break;
        default:
            break;
    }

    // SpriteFontの表示例外を防止するためASCII文字で表示[cite: 43]
    debugRenderer.DrawText({dialogX + textOffsetX, dialogY + 30.0f}, Colors::White, resultStr);
    debugRenderer.DrawText({btnX + 75.0f, btnY1 + 12.0f}, Colors::White, L"RETRY");
    debugRenderer.DrawText({btnX + 75.0f, btnY2 + 12.0f}, Colors::White, L"TITLE");
}