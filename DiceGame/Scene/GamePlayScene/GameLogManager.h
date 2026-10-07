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
#include <format>
#include <utility>

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

    // 通常のログ追加
    void AddLog(const std::wstring& message);

    // 書式付きログ追加 (AddLogFormat)
    template <typename... Args> void AddLogFormat(const wchar_t* fmt, Args&&... args)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            AddLog(fmt);
        }
        else
        {
            std::wstring formattedMsg = std::vformat(fmt, std::make_wformat_args(args...));
            AddLog(formattedMsg);
        }
    }

    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight);

    // 引数7個の Render
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectColor,
                float screenWidth, float screenHeight);

    // 引数9個の Render (呼び出し側の互換用)
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* batchTex,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectTex,
                DirectX::BasicEffect* effectColor, float screenWidth, float screenHeight);

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

private:
    std::vector<std::wstring> m_logs;
    bool m_showLogWindow = false;
    bool m_isButtonHovered = false;
    RECT m_logButtonRect = {0, 0, 0, 0};
};