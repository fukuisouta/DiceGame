#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <Mouse.h>
#include <CommonStates.h>
#include <vector>
#include <string>
#include <cwchar>
#include <cstdarg>

namespace Imase
{
    class DebugRenderer;
}

class GameLogManager
{
public:
    GameLogManager() = default;
    ~GameLogManager() = default;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    void AddLog(const std::wstring& message);

    // ★ C2039/C7595 を完全に回避する安全な AddLogFormat 実装
    void AddLogFormat(const wchar_t* fmt, ...)
    {
        wchar_t buffer[512];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        AddLog(buffer);
    }

    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight);

    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectColor,
                float screenWidth, float screenHeight);

    void ToggleWindow();

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
    std::vector<std::wstring> m_logs;
    bool m_showLogWindow = false;
    bool m_isButtonHovered = false;
    RECT m_logButtonRect = {0, 0, 0, 0};
    int m_scrollOffset = 0;
};