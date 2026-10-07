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

    int maxVisible = 10;
    m_scrollOffset = std::max(0, static_cast<int>(m_logs.size()) - maxVisible);
}

void GameLogManager::Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight)
{
      m_isButtonHovered = (mouseState.x >= m_logButtonRect.left && mouseState.x <= m_logButtonRect.right &&
                         mouseState.y >= m_logButtonRect.top && mouseState.y <= m_logButtonRect.bottom);

    if (m_showLogWindow)
    {
        static int lastScrollWheel = 0;
        int scrollDelta = mouseState.scrollWheelValue - lastScrollWheel;
        lastScrollWheel = mouseState.scrollWheelValue;

        if (scrollDelta != 0)
        {
            int step = 1;
            if (scrollDelta > 0)
                m_scrollOffset -= step;
            else if (scrollDelta < 0)
                m_scrollOffset += step;

            int maxVisible = 10;
            int maxOffset = std::max(0, static_cast<int>(m_logs.size()) - maxVisible);

            m_scrollOffset = std::clamp(m_scrollOffset, 0, maxOffset);
        }
    }
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
    // --- 1. ログウィンドウ背景の描画
    if (m_showLogWindow && batchColor != nullptr && effectColor != nullptr)
    {
        Matrix projection = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
        effectColor->SetProjection(projection);
        effectColor->SetWorld(Matrix::Identity);
        effectColor->SetView(Matrix::Identity);
        effectColor->SetVertexColorEnabled(true);
        effectColor->Apply(context);

        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(device.GetAddressOf());

        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
        void const* shaderByteCode = nullptr;
        size_t byteCodeLength = 0;
        effectColor->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        if (SUCCEEDED(device->CreateInputLayout(VertexPositionColor::InputElements,
                                                VertexPositionColor::InputElementCount, shaderByteCode, byteCodeLength,
                                                inputLayout.GetAddressOf())))
        {
            context->IASetInputLayout(inputLayout.Get());
            context->OMSetBlendState(states.NonPremultiplied(), nullptr, 0xFFFFFFFF);
            context->RSSetState(states.CullNone());
            context->OMSetDepthStencilState(states.DepthNone(), 0);

            batchColor->Begin();

            float winWidth = 400.0f;
            float winHeight = 300.0f;
            float winX = screenWidth - winWidth - 10.0f;
            float winY = 90.0f;

            Vector4 winCol(0.05f, 0.05f, 0.1f, 0.85f);
            VertexPositionColor w0(Vector3(winX, winY, 0.0f), winCol);
            VertexPositionColor w1(Vector3(winX + winWidth, winY, 0.0f), winCol);
            VertexPositionColor w2(Vector3(winX, winY + winHeight, 0.0f), winCol);
            VertexPositionColor w3(Vector3(winX + winWidth, winY + winHeight, 0.0f), winCol);

            batchColor->DrawTriangle(w0, w1, w2);
            batchColor->DrawTriangle(w2, w1, w3);

            batchColor->End();
        }
    }

    // --- 2. ログテキストの描画 ---
    if (m_showLogWindow)
    {
        float winX = screenWidth - 400.0f - 10.0f;
        float startY = 100.0f;
        int maxVisible = 10;

        int startIdx = m_scrollOffset;
        int endIdx = std::min(static_cast<int>(m_logs.size()), startIdx + maxVisible);

        for (int i = startIdx; i < endIdx; ++i)
        {
            float lineY = startY + (i - startIdx) * 26.0f;
            debugRenderer.DrawText(Vector2(winX + 15.0f, lineY), Colors::Yellow, m_logs[i]);
        }
    }
}