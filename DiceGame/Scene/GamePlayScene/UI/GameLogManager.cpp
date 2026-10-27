#include "pch.h"
#include "GameLogManager.h"
#include "ImaseLib/DebugRenderer.h"
#include <WICTextureLoader.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 初期化処理
void GameLogManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    (void)context;
    m_logs.clear();
    m_showLogWindow = false;
    m_isButtonHovered = false;

    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/LOG.png", nullptr,
                                      m_logButtonTexture.ReleaseAndGetAddressOf());
}

// ログ文字列の追加（19文字超えでの自動改行整形を含む）
void GameLogManager::AddLog(const std::wstring& message)
{
    // 1行あたりの最大文字数を 19 に拡張（ウィンドウ幅に綺麗に収まる長さ）[cite: 58]
    const size_t maxCharsPerLine = 19;

    std::wstring wrappedMessage;
    size_t count = 0;

    for (size_t i = 0; i < message.length(); ++i)
    {
        wchar_t ch = message[i];

        // 既存の改行文字があればカウントをリセット[cite: 58]
        if (ch == L'\n')
        {
            count = 0;
            wrappedMessage += ch;
            continue;
        }

        // 指定文字数を超えた場合のみ自動改行[cite: 58]
        if (count >= maxCharsPerLine)
        {
            wrappedMessage += L'\n';
            count = 0;
        }

        wrappedMessage += ch;
        count++;
    }

    m_logs.push_back(wrappedMessage);

    int maxVisible = 10;
    m_scrollOffset = std::max(0, static_cast<int>(m_logs.size()) - maxVisible);
}

// 毎フレームのマウスホバーおよびスクロールホイール操作更新
void GameLogManager::Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight)
{
    (void)screenWidth;
    (void)screenHeight;

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

// ログウィンドウ開閉表示の切替
void GameLogManager::ToggleWindow()
{
    m_showLogWindow = !m_showLogWindow;
}

// ボタンクリック判定
bool GameLogManager::IsButtonClicked(DirectX::Mouse::ButtonStateTracker& tracker) const
{
    return m_isButtonHovered && (tracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED);
}

// LOGボタンアイコンの描画
void GameLogManager::RenderButton(ID3D11DeviceContext* context, PrimitiveBatch<VertexPositionTexture>* batch,
                                  BasicEffect* effect, ID3D11InputLayout* inputLayout)
{
    if (!m_logButtonTexture || !batch || !effect)
        return;

    float baseW = static_cast<float>(m_logButtonRect.right - m_logButtonRect.left);
    float baseH = static_cast<float>(m_logButtonRect.bottom - m_logButtonRect.top);
    float scale = m_isButtonHovered ? 1.05f : 1.0f;

    float bWidth = baseW * scale;
    float bHeight = baseH * scale;

    float btnX = static_cast<float>(m_logButtonRect.left) - (bWidth - baseW) * 0.5f;
    float btnY = static_cast<float>(m_logButtonRect.top) - (bHeight - baseH) * 0.5f;

    context->IASetInputLayout(inputLayout);
    effect->SetTexture(m_logButtonTexture.Get());
    effect->Apply(context);

    batch->Begin();
    VertexPositionTexture v0(Vector3(btnX, btnY, 0.0f), Vector2(0.0f, 0.0f));
    VertexPositionTexture v1(Vector3(btnX + bWidth, btnY, 0.0f), Vector2(1.0f, 0.0f));
    VertexPositionTexture v2(Vector3(btnX, btnY + bHeight, 0.0f), Vector2(0.0f, 1.0f));
    VertexPositionTexture v3(Vector3(btnX + bWidth, btnY + bHeight, 0.0f), Vector2(1.0f, 1.0f));
    batch->DrawTriangle(v0, v1, v2);
    batch->DrawTriangle(v2, v1, v3);
    batch->End();
}

// ログウィンドウ背景およびログテキストの描画
void GameLogManager::Render(ID3D11DeviceContext* context, CommonStates& states, Imase::DebugRenderer& debugRenderer,
                            PrimitiveBatch<VertexPositionColor>* batchColor, BasicEffect* effectColor,
                            float screenWidth, float screenHeight)
{
    if (!m_showLogWindow || batchColor == nullptr || effectColor == nullptr)
        return;

    // 2D正射影行列の設定とエフェクト設定[cite: 58]
    Matrix projection = Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f);
    effectColor->SetProjection(projection);
    effectColor->SetWorld(Matrix::Identity);
    effectColor->SetView(Matrix::Identity);
    effectColor->SetVertexColorEnabled(true);
    effectColor->SetTextureEnabled(false);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    void const* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    effectColor->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    if (SUCCEEDED(device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                            shaderByteCode, byteCodeLength, inputLayout.GetAddressOf())))
    {
        context->IASetInputLayout(inputLayout.Get());
        context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
        context->RSSetState(states.CullNone());
        context->OMSetDepthStencilState(states.DepthNone(), 0);

        effectColor->Apply(context);

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

    // ログテキスト描画[cite: 58]
    float winX = screenWidth - 400.0f - 10.0f;
    float startY = 100.0f;

    int startIdx = m_scrollOffset;
    int endIdx = std::min(static_cast<int>(m_logs.size()), startIdx + 10);

    float currentY = startY;
    const float lineSpacing = 28.0f; // 行間を28px開けて文字重なりを防止[cite: 58]

    for (int i = startIdx; i < endIdx; ++i)
    {
        if (currentY > startY + 260.0f)
            break;

        debugRenderer.DrawText(Vector2(winX + 15.0f, currentY), Colors::Yellow, m_logs[i]);

        // ログに含まれる改行('\n')の数をカウントしてY座標を計算加算[cite: 58]
        size_t lineCount = 1;
        for (wchar_t ch : m_logs[i])
        {
            if (ch == L'\n')
            {
                lineCount++;
            }
        }

        currentY += lineCount * lineSpacing;
    }
}