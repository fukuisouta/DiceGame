#include "pch.h"
#include "GameLogManager.h"
#include "ImaseLib/DebugRenderer.h"
#include <WICTextureLoader.h>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// チャット形式ログ描画スタイル定数定義
namespace ChatLogStyle
{
    constexpr size_t MAX_CHARS_PER_LINE = 28; // 吹き出し内1行あたりの最大文字数
    constexpr float HOVER_SCALE = 1.05f;      // ボタンホバー時の拡大倍率

    constexpr float PANEL_MARGIN_X = 40.0f;      // 画面左右の余白
    constexpr float PANEL_MARGIN_TOP = 90.0f;    // 上部余白（LOGボタンを避ける高さ）
    constexpr float PANEL_MARGIN_BOTTOM = 30.0f; // 下部余白
    constexpr float HEADER_HEIGHT = 44.0f;       // ヘッダーバー高さ
    constexpr float STATUS_BAR_HEIGHT = 36.0f;   // 手数・陣地状況ステータスバー高さ

    constexpr float CHAT_SIDE_PADDING = 36.0f;      // パネル内側の左右余白
    constexpr float BUBBLE_PAD_X = 20.0f;           // 吹き出し内の左右パディング
    constexpr float BUBBLE_PAD_Y = 12.0f;           // 吹き出し内の上下パディング
    constexpr float LINE_HEIGHT = 26.0f;            // 1行あたりのテキスト高さ
    constexpr float FULLWIDTH_CHAR_W = 22.5f;       // 全角1文字あたりの描画幅
    constexpr float HALFWIDTH_CHAR_W = 12.5f;       // 半角1文字あたりの描画幅
    constexpr float BUBBLE_MIN_WIDTH = 160.0f;      // 吹き出し最小幅
    constexpr float BUBBLE_MAX_WIDTH_RATIO = 0.68f; // パネル幅に対する吹き出し最大幅割合
    constexpr float ENTRY_GAP_Y = 20.0f;            // 吹き出し同士の縦間隔
    constexpr float SPEAKER_LABEL_H = 24.0f;        // 「YOU / OPPONENT」ラベル高さ
    constexpr float TAIL_SIZE = 12.0f;              // 吹き出しの三角しっぽサイズ
}

namespace
{
    // テキスト各行の最大推定ピクセル幅を計算するヘルパー（全角/半角区別）
    float MeasureTextPixelWidth(const std::wstring& text)
    {
        float maxW = 0.0f;
        float curW = 0.0f;
        for (wchar_t ch : text)
        {
            if (ch == L'\n')
            {
                maxW = std::max(maxW, curW);
                curW = 0.0f;
            }
            else if (ch <= 0x007F)
            {
                // ASCII（半角英数字・記号・スペース）
                curW += ChatLogStyle::HALFWIDTH_CHAR_W;
            }
            else
            {
                // 日本語・全角記号
                curW += ChatLogStyle::FULLWIDTH_CHAR_W;
            }
        }
        return std::max(maxW, curW);
    }
}

// 初期化処理
void GameLogManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    (void)context;
    m_logs.clear();
    m_showLogWindow = false;
    m_isButtonHovered = false;
    m_scrollY = 0.0f;
    m_maxScrollY = 0.0f;

    DirectX::CreateWICTextureFromFile(device, L"Resources/Textures/LOG.png", nullptr,
                                      m_logButtonTexture.ReleaseAndGetAddressOf());
}

// ログ文字列の追加（発言者判定＋自動折り返し）
void GameLogManager::AddLog(const std::wstring& message)
{
    LogEntry entry;

    // 先頭文字列から発言者（自分／相手／システム）を自動判定
    if (message.rfind(L"あなた", 0) == 0)
    {
        entry.speaker = LogSpeaker::Self;
    }
    else if (message.rfind(L"あいて", 0) == 0 || message.rfind(L"相手", 0) == 0)
    {
        entry.speaker = LogSpeaker::Enemy;
    }
    else
    {
        entry.speaker = LogSpeaker::System;
    }

    // 改行を整理しつつ1行の最大文字数で折り返す
    std::wstring wrapped;
    size_t currentChars = 0;
    size_t lineCount = 1;
    size_t maxChars = 1;

    for (size_t i = 0; i < message.length(); ++i)
    {
        wchar_t ch = message[i];
        if (ch == L'\n')
        {
            maxChars = std::max(maxChars, currentChars);
            currentChars = 0;
            lineCount++;
            wrapped += ch;
            continue;
        }

        if (currentChars >= ChatLogStyle::MAX_CHARS_PER_LINE)
        {
            maxChars = std::max(maxChars, currentChars);
            wrapped += L'\n';
            currentChars = 0;
            lineCount++;
        }

        wrapped += ch;
        currentChars++;
    }
    maxChars = std::max(maxChars, currentChars);

    entry.text = wrapped;
    entry.lineCount = lineCount;
    entry.maxLineChars = maxChars;

    m_logs.push_back(entry);

    // 新しいログが追加されたら一番下（最新）へ自動スクロール
    m_scrollY = 0.0f;
}

// 毎フレームのマウスホバーおよびスクロールホイール操作更新
void GameLogManager::Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight)
{
    (void)screenWidth;
    (void)screenHeight;

    m_isButtonHovered = (mouseState.x >= m_logButtonRect.left && mouseState.x <= m_logButtonRect.right &&
                         mouseState.y >= m_logButtonRect.top && mouseState.y <= m_logButtonRect.bottom);

    static int lastScrollWheel = mouseState.scrollWheelValue;
    int scrollDelta = mouseState.scrollWheelValue - lastScrollWheel;
    lastScrollWheel = mouseState.scrollWheelValue;

    if (m_showLogWindow && scrollDelta != 0)
    {
        float scrollSpeed = 48.0f;
        m_scrollY += (scrollDelta > 0) ? scrollSpeed : -scrollSpeed;
        m_scrollY = std::clamp(m_scrollY, 0.0f, m_maxScrollY);
    }
}

// ログウィンドウ開閉表示の切替
void GameLogManager::ToggleWindow()
{
    m_showLogWindow = !m_showLogWindow;
    if (m_showLogWindow)
    {
        m_scrollY = 0.0f;
    }
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
    float scale = m_isButtonHovered ? ChatLogStyle::HOVER_SCALE : 1.0f;

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

// 全画面チャット形式ログウィンドウの描画
void GameLogManager::Render(ID3D11DeviceContext* context, CommonStates& states, Imase::DebugRenderer& debugRenderer,
                            PrimitiveBatch<VertexPositionColor>* batchColor, BasicEffect* effectColor,
                            float screenWidth, float screenHeight, int currentTurn, int maxTurns, int p1PaintedTiles,
                            int p2PaintedTiles)
{
    if (!m_showLogWindow || batchColor == nullptr || effectColor == nullptr)
        return;

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

    if (FAILED(device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                         shaderByteCode, byteCodeLength, inputLayout.GetAddressOf())))
    {
        return;
    }

    context->IASetInputLayout(inputLayout.Get());
    context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->RSSetState(states.CullNone());
    context->OMSetDepthStencilState(states.DepthNone(), 0);
    effectColor->Apply(context);

    auto DrawRect = [&](float x, float y, float w, float h, const Vector4& col)
    {
        VertexPositionColor v0(Vector3(x, y, 0.0f), col);
        VertexPositionColor v1(Vector3(x + w, y, 0.0f), col);
        VertexPositionColor v2(Vector3(x, y + h, 0.0f), col);
        VertexPositionColor v3(Vector3(x + w, y + h, 0.0f), col);
        batchColor->DrawTriangle(v0, v1, v2);
        batchColor->DrawTriangle(v2, v1, v3);
    };

    float panelX = ChatLogStyle::PANEL_MARGIN_X;
    float panelY = ChatLogStyle::PANEL_MARGIN_TOP;
    float panelW = std::max(300.0f, screenWidth - ChatLogStyle::PANEL_MARGIN_X * 2.0f);
    float panelH = std::max(200.0f, screenHeight - ChatLogStyle::PANEL_MARGIN_TOP - ChatLogStyle::PANEL_MARGIN_BOTTOM);

    float statusBarY = panelY + ChatLogStyle::HEADER_HEIGHT;
    float contentTop = statusBarY + ChatLogStyle::STATUS_BAR_HEIGHT + 12.0f;
    float contentBottom = panelY + panelH - 12.0f;
    float contentHeight = std::max(50.0f, contentBottom - contentTop);
    float maxBubbleW = panelW * ChatLogStyle::BUBBLE_MAX_WIDTH_RATIO;

    struct LayoutItem
    {
        size_t logIndex;
        float boxX;
        float boxY;
        float boxW;
        float boxH;
        float totalBlockH;
    };

    std::vector<LayoutItem> layoutItems;
    layoutItems.reserve(m_logs.size());

    float totalContentH = 0.0f;
    for (size_t i = 0; i < m_logs.size(); ++i)
    {
        const auto& entry = m_logs[i];

        // 全角・半角を考慮した実ピクセル幅＋余裕分(+16px)で吹き出し幅を決定
        float textW = MeasureTextPixelWidth(entry.text) + 16.0f;
        float boxW = std::clamp(textW + ChatLogStyle::BUBBLE_PAD_X * 2.0f, ChatLogStyle::BUBBLE_MIN_WIDTH, maxBubbleW);
        float boxH =
            static_cast<float>(entry.lineCount) * ChatLogStyle::LINE_HEIGHT + ChatLogStyle::BUBBLE_PAD_Y * 2.0f;

        float labelH = (entry.speaker == LogSpeaker::System) ? 0.0f : ChatLogStyle::SPEAKER_LABEL_H;
        float blockH = labelH + boxH;

        LayoutItem item{};
        item.logIndex = i;
        item.boxW = boxW;
        item.boxH = boxH;
        item.totalBlockH = blockH;
        item.boxY = totalContentH + labelH;

        if (entry.speaker == LogSpeaker::Self)
        {
            item.boxX = (panelX + panelW) - ChatLogStyle::CHAT_SIDE_PADDING - boxW;
        }
        else if (entry.speaker == LogSpeaker::Enemy)
        {
            item.boxX = panelX + ChatLogStyle::CHAT_SIDE_PADDING;
        }
        else
        {
            item.boxX = panelX + (panelW - boxW) * 0.5f;
        }

        layoutItems.push_back(item);
        totalContentH += blockH + ChatLogStyle::ENTRY_GAP_Y;
    }

    if (totalContentH > ChatLogStyle::ENTRY_GAP_Y)
    {
        totalContentH -= ChatLogStyle::ENTRY_GAP_Y;
    }

    m_maxScrollY = std::max(0.0f, totalContentH - contentHeight);
    m_scrollY = std::clamp(m_scrollY, 0.0f, m_maxScrollY);

    float baseOffsetY = (totalContentH <= contentHeight) ? contentTop : (contentBottom - totalContentH + m_scrollY);

    // --- 1. 背景パネル・ステータスバー・吹き出しポリゴンの一括描画 ---
    batchColor->Begin();

    DrawRect(0.0f, 0.0f, screenWidth, screenHeight, Vector4(0.0f, 0.0f, 0.0f, 0.45f));

    DrawRect(panelX - 2.0f, panelY - 2.0f, panelW + 4.0f, panelH + 4.0f, Vector4(0.35f, 0.45f, 0.65f, 0.9f));
    DrawRect(panelX, panelY, panelW, panelH, Vector4(0.06f, 0.08f, 0.14f, 0.92f));
    DrawRect(panelX, panelY, panelW, ChatLogStyle::HEADER_HEIGHT, Vector4(0.12f, 0.16f, 0.28f, 0.98f));

    // 手数・陣地状況ステータスバー背景
    DrawRect(panelX, statusBarY, panelW, ChatLogStyle::STATUS_BAR_HEIGHT, Vector4(0.09f, 0.12f, 0.20f, 0.96f));
    DrawRect(panelX, statusBarY + ChatLogStyle::STATUS_BAR_HEIGHT - 1.0f, panelW, 1.0f,
             Vector4(0.35f, 0.45f, 0.65f, 0.65f));

    for (const auto& item : layoutItems)
    {
        const auto& entry = m_logs[item.logIndex];
        float drawY = baseOffsetY + item.boxY;
        float topEdge = drawY - ((entry.speaker == LogSpeaker::System) ? 0.0f : ChatLogStyle::SPEAKER_LABEL_H);
        float bottomEdge = drawY + item.boxH;

        if (topEdge < contentTop || bottomEdge > contentBottom)
            continue;

        if (entry.speaker == LogSpeaker::Self)
        {
            Vector4 borderCol(0.30f, 0.65f, 1.0f, 0.95f);
            Vector4 fillCol(0.12f, 0.32f, 0.58f, 0.95f);

            DrawRect(item.boxX - 2.0f, drawY - 2.0f, item.boxW + 4.0f, item.boxH + 4.0f, borderCol);
            DrawRect(item.boxX, drawY, item.boxW, item.boxH, fillCol);

            float tailX = item.boxX + item.boxW;
            float tailY = drawY + 14.0f;
            VertexPositionColor t0(Vector3(tailX, tailY, 0.0f), borderCol);
            VertexPositionColor t1(Vector3(tailX + ChatLogStyle::TAIL_SIZE, tailY + 6.0f, 0.0f), borderCol);
            VertexPositionColor t2(Vector3(tailX, tailY + 14.0f, 0.0f), borderCol);
            batchColor->DrawTriangle(t0, t1, t2);
        }
        else if (entry.speaker == LogSpeaker::Enemy)
        {
            Vector4 borderCol(1.0f, 0.45f, 0.35f, 0.95f);
            Vector4 fillCol(0.48f, 0.16f, 0.16f, 0.95f);

            DrawRect(item.boxX - 2.0f, drawY - 2.0f, item.boxW + 4.0f, item.boxH + 4.0f, borderCol);
            DrawRect(item.boxX, drawY, item.boxW, item.boxH, fillCol);

            float tailX = item.boxX;
            float tailY = drawY + 14.0f;
            VertexPositionColor t0(Vector3(tailX, tailY, 0.0f), borderCol);
            VertexPositionColor t1(Vector3(tailX, tailY + 14.0f, 0.0f), borderCol);
            VertexPositionColor t2(Vector3(tailX - ChatLogStyle::TAIL_SIZE, tailY + 6.0f, 0.0f), borderCol);
            batchColor->DrawTriangle(t0, t1, t2);
        }
        else
        {
            Vector4 borderCol(0.85f, 0.72f, 0.25f, 0.9f);
            Vector4 fillCol(0.18f, 0.17f, 0.12f, 0.92f);

            DrawRect(item.boxX - 1.5f, drawY - 1.5f, item.boxW + 3.0f, item.boxH + 3.0f, borderCol);
            DrawRect(item.boxX, drawY, item.boxW, item.boxH, fillCol);
        }
    }

    batchColor->End();

    // --- 2. ヘッダータイトル・手数＆陣地状況・吹き出し内テキストの描画 ---
    debugRenderer.DrawText(Vector2(panelX + 20.0f, panelY + 10.0f), Vector4(0.9f, 0.95f, 1.0f, 1.0f),
                           L"BATTLE LOG  (ホイールでスクロール / LOGボタンで閉じる)");

    int remainingTurns = std::max(0, maxTurns - currentTurn);
    wchar_t statusBuf[128] = {};
    swprintf_s(statusBuf, L"【現在手数】 %d / %d手 (残り %d手)      【陣地塗り】 青(1P): %dマス  vs  赤(2P): %dマス",
               currentTurn, maxTurns, remainingTurns, p1PaintedTiles, p2PaintedTiles);
    debugRenderer.DrawText(Vector2(panelX + 20.0f, statusBarY + 6.0f), Vector4(0.65f, 0.92f, 1.0f, 1.0f), statusBuf);

    for (const auto& item : layoutItems)
    {
        const auto& entry = m_logs[item.logIndex];
        float drawY = baseOffsetY + item.boxY;
        float topEdge = drawY - ((entry.speaker == LogSpeaker::System) ? 0.0f : ChatLogStyle::SPEAKER_LABEL_H);
        float bottomEdge = drawY + item.boxH;

        if (topEdge < contentTop || bottomEdge > contentBottom)
            continue;

        if (entry.speaker == LogSpeaker::Self)
        {
            debugRenderer.DrawText(Vector2(item.boxX + item.boxW - 52.0f, topEdge - 2.0f),
                                   Vector4(0.55f, 0.85f, 1.0f, 1.0f), L"YOU");
            debugRenderer.DrawText(Vector2(item.boxX + ChatLogStyle::BUBBLE_PAD_X, drawY + ChatLogStyle::BUBBLE_PAD_Y),
                                   Colors::White, entry.text);
        }
        else if (entry.speaker == LogSpeaker::Enemy)
        {
            debugRenderer.DrawText(Vector2(item.boxX, topEdge - 2.0f), Vector4(1.0f, 0.65f, 0.55f, 1.0f), L"OPPONENT");
            debugRenderer.DrawText(Vector2(item.boxX + ChatLogStyle::BUBBLE_PAD_X, drawY + ChatLogStyle::BUBBLE_PAD_Y),
                                   Colors::White, entry.text);
        }
        else
        {
            debugRenderer.DrawText(Vector2(item.boxX + ChatLogStyle::BUBBLE_PAD_X, drawY + ChatLogStyle::BUBBLE_PAD_Y),
                                   Vector4(1.0f, 0.92f, 0.45f, 1.0f), entry.text);
        }
    }

    effectColor->SetWorld(Matrix::Identity);
    effectColor->SetView(Matrix::Identity);
    effectColor->SetProjection(Matrix::Identity);
    context->OMSetDepthStencilState(states.DepthDefault(), 0);
}