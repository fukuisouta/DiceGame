#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <Mouse.h>
#include <CommonStates.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cwchar>
#include <cstdarg>

namespace Imase
{
    class DebugRenderer;
}

// ============================================================================
// 対戦行動ログの蓄積・自動折り返し改行・スクロール表示およびUIボタン管理クラス
// ============================================================================
class GameLogManager
{
public:
    GameLogManager() = default;
    ~GameLogManager() = default;

    // リソース・テクスチャの初期化
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ログメッセージの追加（自動折り返し処理付き）
    void AddLog(const std::wstring& message);

    // フォーマット付きログメッセージ追加
    void AddLogFormat(const wchar_t* fmt, ...)
    {
        wchar_t buffer[512];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        AddLog(buffer);
    }

    // マウススクロール・ボタンホバーの更新
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight);

    // ログ表示ウィンドウの描画
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectColor,
                float screenWidth, float screenHeight);

    // LOG表示切替ボタン（LOG.png）の描画[cite: 59]
    void RenderButton(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* batch,
                      DirectX::BasicEffect* effect, ID3D11InputLayout* inputLayout);

    // ウィンドウ開閉切替
    void ToggleWindow();

    // 判定ゲッター・セッター
    bool IsButtonHovered() const
    {
        return m_isButtonHovered;
    }
    bool IsButtonClicked(DirectX::Mouse::ButtonStateTracker& tracker) const;
    bool IsWindowVisible() const
    {
        return m_showLogWindow;
    }

    const RECT& GetButtonRect() const
    {
        return m_logButtonRect;
    }

    void SetButtonRect(const RECT& rect)
    {
        m_logButtonRect = rect;
    }

private:
    std::vector<std::wstring> m_logs; // ログテキスト履歴
    bool m_showLogWindow = false;     // ウィンドウ表示フラグ
    bool m_isButtonHovered = false;   // ボタンホバーフラグ
    RECT m_logButtonRect = {0, 0, 0, 0};
    int m_scrollOffset = 0; // スクロール位置

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture; // LOGボタンテクスチャ[cite: 59]
};