#include "pch.h"
#include "GameLogManager.h"
#include "ImaseLib/DebugRenderer.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void GameLogManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_logs.clear();
    m_showLogWindow = false;
    m_isButtonHovered = false;
}

void GameLogManager::AddLog(const std::wstring& message)
{
    m_logs.push_back(message);
    if (m_logs.size() > 50) // 最大ログ保持数
    {
        m_logs.erase(m_logs.begin());
    }
}

void GameLogManager::Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight)
{
    // 右上にログボタンを配置（幅 80px, 高さ 30px）
    float btnWidth = 80.0f;
    float btnHeight = 30.0f;
    float btnX = screenWidth - btnWidth - 10.0f;
    float btnY = 40.0f;

    m_logButtonRect = {static_cast<LONG>(btnX), static_cast<LONG>(btnY), static_cast<LONG>(btnX + btnWidth),
                       static_cast<LONG>(btnY + btnHeight)};

    // マウスカーソルがボタン領域内にあるか判定
    m_isButtonHovered = (mouseState.x >= m_logButtonRect.left && mouseState.x <= m_logButtonRect.right &&
                         mouseState.y >= m_logButtonRect.top && mouseState.y <= m_logButtonRect.bottom);
}

void GameLogManager::ToggleWindow()
{
    m_showLogWindow = !m_showLogWindow;
}

bool GameLogManager::IsButtonClicked(DirectX::Mouse::ButtonStateTracker& tracker) const
{
    return m_isButtonHovered && (tracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED);
}

void GameLogManager::Render(ID3D11DeviceContext* context, CommonStates& states, Imase::DebugRenderer& debugRenderer,
                            PrimitiveBatch<VertexPositionColor>* batchColor, BasicEffect* effectColor,
                            float screenWidth, float screenHeight)
{
    // --- ポインタチェック (NULL 事故防止) ---
    if (batchColor != nullptr && effectColor != nullptr)
    {
        // 2D 描画用にスクリーンの正射影行列を設定
        Matrix projection = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
        effectColor->SetProjection(projection);
        effectColor->SetWorld(Matrix::Identity);
        effectColor->SetView(Matrix::Identity);
        effectColor->SetVertexColorEnabled(true);

        // 1. シェーダーパスの適用
        effectColor->Apply(context);

        // 2. VertexPositionColor 用の InputLayout を取得して設定
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(device.GetAddressOf());

        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
        void const* shaderByteCode = nullptr;
        size_t byteCodeLength = 0;
        effectColor->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        HRESULT hr =
            device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                      shaderByteCode, byteCodeLength, inputLayout.GetAddressOf());

        if (SUCCEEDED(hr))
        {
            context->IASetInputLayout(inputLayout.Get());

            // パイプライン状態の設定
            context->OMSetBlendState(states.NonPremultiplied(), nullptr, 0xFFFFFFFF);
            context->RSSetState(states.CullNone());
            context->OMSetDepthStencilState(states.DepthNone(), 0);

            // 描画開始
            batchColor->Begin();

            // ボタン背景の描画
            Vector4 btnCol = m_isButtonHovered ? Vector4(0.3f, 0.6f, 0.9f, 0.9f) : Vector4(0.2f, 0.2f, 0.3f, 0.8f);
            VertexPositionColor v0(Vector3((float)m_logButtonRect.left, (float)m_logButtonRect.top, 0.0f), btnCol);
            VertexPositionColor v1(Vector3((float)m_logButtonRect.right, (float)m_logButtonRect.top, 0.0f), btnCol);
            VertexPositionColor v2(Vector3((float)m_logButtonRect.left, (float)m_logButtonRect.bottom, 0.0f), btnCol);
            VertexPositionColor v3(Vector3((float)m_logButtonRect.right, (float)m_logButtonRect.bottom, 0.0f), btnCol);

            batchColor->DrawTriangle(v0, v1, v2);
            batchColor->DrawTriangle(v2, v1, v3);

            // ログウィンドウ背景の描画（開いている場合）
            if (m_showLogWindow)
            {
                float winWidth = 400.0f;
                float winHeight = 300.0f;
                float winX = screenWidth - winWidth - 10.0f;
                float winY = 80.0f;

                Vector4 winCol(0.05f, 0.05f, 0.1f, 0.85f);
                VertexPositionColor w0(Vector3(winX, winY, 0.0f), winCol);
                VertexPositionColor w1(Vector3(winX + winWidth, winY, 0.0f), winCol);
                VertexPositionColor w2(Vector3(winX, winY + winHeight, 0.0f), winCol);
                VertexPositionColor w3(Vector3(winX + winWidth, winY + winHeight, 0.0f), winCol);

                batchColor->DrawTriangle(w0, w1, w2);
                batchColor->DrawTriangle(w2, w1, w3);
            }

            batchColor->End();
        }
    }

    // --- 2. DebugRenderer へのテキスト描画予約 ---
    debugRenderer.DrawText(Vector2((float)m_logButtonRect.left + 22.0f, (float)m_logButtonRect.top + 5.0f),
                           Colors::White, L"log");

    if (m_showLogWindow)
    {
        float winX = screenWidth - 400.0f - 10.0f;
        float startY = 90.0f;
        int maxVisible = 10;
        int startIdx = std::max(0, static_cast<int>(m_logs.size()) - maxVisible);

        for (size_t i = startIdx; i < m_logs.size(); ++i)
        {
            float lineY = startY + (i - startIdx) * 26.0f;
            debugRenderer.DrawText(Vector2(winX + 15.0f, lineY), Colors::Yellow, m_logs[i]);
        }
    }
}

// 呼び出し互換用（引数9個のRender）
void GameLogManager::Render(ID3D11DeviceContext* context, DirectX::CommonStates& states,
                            Imase::DebugRenderer& debugRenderer,
                            DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* batchTex,
                            DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor,
                            DirectX::BasicEffect* effectTex, DirectX::BasicEffect* effectColor, float screenWidth,
                            float screenHeight)
{
    Render(context, states, debugRenderer, batchColor, effectColor, screenWidth, screenHeight);
}