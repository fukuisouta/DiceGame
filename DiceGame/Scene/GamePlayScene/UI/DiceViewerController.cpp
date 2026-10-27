#include "pch.h"
#include "DiceViewerController.h"
#include "GamePlayUIController.h"
#include "Dice/Dice.h"
#include "ImaseLib/DebugRenderer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 初期化処理
void DiceViewerController::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float aspectRatio)
{
    m_diceViewer.Initialize(aspectRatio);
    m_colorPrimitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
    m_colorEffect = std::make_unique<BasicEffect>(device);
    m_colorEffect->SetTextureEnabled(false);
    m_colorEffect->SetVertexColorEnabled(true);
}

// 画面を開く
void DiceViewerController::Open(Dice* targetDice)
{
    m_diceViewer.Open();
    if (targetDice)
        m_diceViewer.ResetCameraWithRotation(targetDice->GetRotation());
    else
        m_diceViewer.ResetCamera();
}

// ダイスチェック画面の入力操作・ボタン操作の更新
bool DiceViewerController::Update(float dt, const Mouse::State& mouseState,
                                  const Mouse::ButtonStateTracker& mouseTracker,
                                  const Keyboard::KeyboardStateTracker& kbTracker,
                                  const GamePlayUIController& uiController, Dice* targetDice, float screenWidth,
                                  float screenHeight)
{
    // ボタンホバーからダイスチェック画面を開く[cite: 56]
    if (!IsOpen())
    {
        if (uiController.IsDiceCheckBtnHovered() && mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
        {
            Open(targetDice);
            return true;
        }
        return false;
    }

    // 開いている状態の更新処理[cite: 56]
    m_diceViewer.SetAspectRatio(screenWidth / screenHeight);
    m_diceViewer.Update(mouseState, dt);

    if (mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
    {
        if (uiController.IsDiceCheckCloseHovered())
        {
            Close();
            return true;
        }
        if (uiController.IsDiceCheckResetHovered())
        {
            if (targetDice)
                m_diceViewer.ResetCameraWithRotation(targetDice->GetRotation());
            else
                m_diceViewer.ResetCamera();
        }
    }

    // ESCキー押下でダイスチェック画面を閉じる
    if (kbTracker.IsKeyPressed(Keyboard::Keys::Escape))
    {
        Close();
        return true;
    }

    return true;
}

// ダイスチェック用オーバーレイ背景・テキスト・3Dダイスの描画
void DiceViewerController::Render(ID3D11DeviceContext* context, CommonStates& states,
                                  Imase::DebugRenderer& debugRenderer, const GamePlayUIController& uiController,
                                  Dice* targetDice, float screenWidth, float screenHeight)
{
    if (!IsOpen())
        return;

    Matrix orthoProj = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);

    m_colorEffect->SetWorld(Matrix::Identity);
    m_colorEffect->SetView(Matrix::Identity);
    m_colorEffect->SetProjection(orthoProj);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    void const* colorByteCode;
    size_t colorByteCodeLength;
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

    // 半透明背景描画[cite: 56]
    Vector4 bgCol(0.0f, 0.0f, 0.0f, 0.75f);
    VertexPositionColor bg0(Vector3(0.0f, 0.0f, 0.0f), bgCol);
    VertexPositionColor bg1(Vector3(screenWidth, 0.0f, 0.0f), bgCol);
    VertexPositionColor bg2(Vector3(0.0f, screenHeight, 0.0f), bgCol);
    VertexPositionColor bg3(Vector3(screenWidth, screenHeight, 0.0f), bgCol);
    m_colorPrimitiveBatch->DrawTriangle(bg0, bg1, bg2);
    m_colorPrimitiveBatch->DrawTriangle(bg2, bg1, bg3);

    // リセット / 閉じるボタンの枠描画[cite: 56]
    RECT resetRect = {static_cast<LONG>(screenWidth - 270), 30, static_cast<LONG>(screenWidth - 150), 70};
    RECT closeRect = {static_cast<LONG>(screenWidth - 130), 30, static_cast<LONG>(screenWidth - 10), 70};

    Vector4 resetCol =
        uiController.IsDiceCheckResetHovered() ? Vector4(0.8f, 0.8f, 0.2f, 0.9f) : Vector4(0.4f, 0.4f, 0.4f, 0.8f);
    Vector4 closeCol =
        uiController.IsDiceCheckCloseHovered() ? Vector4(0.9f, 0.3f, 0.3f, 0.9f) : Vector4(0.6f, 0.2f, 0.2f, 0.8f);

    auto DrawBox = [&](const RECT& rect, Vector4 col)
    {
        VertexPositionColor v0(Vector3((float)rect.left, (float)rect.top, 0), col);
        VertexPositionColor v1(Vector3((float)rect.right, (float)rect.top, 0), col);
        VertexPositionColor v2(Vector3((float)rect.left, (float)rect.bottom, 0), col);
        VertexPositionColor v3(Vector3((float)rect.right, (float)rect.bottom, 0), col);
        m_colorPrimitiveBatch->DrawTriangle(v0, v1, v2);
        m_colorPrimitiveBatch->DrawTriangle(v2, v1, v3);
    };

    DrawBox(resetRect, resetCol);
    DrawBox(closeRect, closeCol);
    m_colorPrimitiveBatch->End();

    // 説明テキスト描画[cite: 56]
    debugRenderer.DrawText({30.0f, 30.0f}, DirectX::Colors::White, L"【ダイスチェックモード】");
    debugRenderer.DrawText({30.0f, 60.0f}, DirectX::Colors::White, L"ドラッグ: 自由回転 | ホイール: 拡大・縮小");
    debugRenderer.DrawText({static_cast<float>(screenWidth - 250), 40.0f}, DirectX::Colors::White, L"リセット");
    debugRenderer.DrawText({static_cast<float>(screenWidth - 100), 40.0f}, DirectX::Colors::White, L"閉じる");

    // 3Dダイスモデル描画
    context->OMSetDepthStencilState(states.DepthDefault(), 0);

    Matrix viewerView = m_diceViewer.GetViewMatrix();
    Matrix viewerProj = m_diceViewer.GetProjectionMatrix();
    Matrix viewerWorld = m_diceViewer.GetWorldMatrix();

    if (targetDice)
    {
        targetDice->Render(context, states, viewerWorld, viewerView, viewerProj);
    }
}