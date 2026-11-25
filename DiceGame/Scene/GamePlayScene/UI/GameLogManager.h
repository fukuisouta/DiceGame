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

// ログマネージャー定数定義
namespace LogConstants
{
    constexpr size_t FORMAT_BUFFER_SIZE = 512; // ログフォーマット用バッファサイズ
}

// 対戦行動ログの蓄積・チャット吹き出し表示およびUIボタン管理クラス
class GameLogManager
{
public:
    // ログの発言者区分
    enum class LogSpeaker
    {
        Self,  // 自分（右側吹き出し）
        Enemy, // 相手（左側吹き出し）
        System // システム通知（中央バナー）
    };

    // 1件分のログデータ構造体
    struct LogEntry
    {
        LogSpeaker speaker = LogSpeaker::System;
        std::wstring text;
        size_t lineCount = 1;
        size_t maxLineChars = 1;
    };

    GameLogManager() = default;
    ~GameLogManager() = default;

    // リソース・テクスチャの初期化を行う関数
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ログメッセージの追加（自動折り返し・発言者判定付き）を行う関数
    void AddLog(const std::wstring& message);

    // フォーマット付きログメッセージを追加する関数
    void AddLogFormat(const wchar_t* fmt, ...)
    {
        wchar_t buffer[LogConstants::FORMAT_BUFFER_SIZE];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        AddLog(buffer);
    }

    // マウススクロール・ボタンホバーの更新を行う関数
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight);

    // ログ表示ウィンドウの描画を行う関数（手数・陣地ステータス表示対応）
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectColor,
                float screenWidth, float screenHeight, int currentTurn = 0, int maxTurns = 20, int p1PaintedTiles = 0,
                int p2PaintedTiles = 0);

    // LOG表示切替ボタン（LOG.png）の描画を行う関数
    void RenderButton(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* batch,
                      DirectX::BasicEffect* effect, ID3D11InputLayout* inputLayout);

    // ウィンドウ開閉状態を切り替える関数
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
    std::vector<LogEntry> m_logs;        // チャットログ履歴
    bool m_showLogWindow = false;        // ウィンドウ表示フラグ
    bool m_isButtonHovered = false;      // ボタンホバーフラグ
    RECT m_logButtonRect = {0, 0, 0, 0}; // ボタン判定領域
    float m_scrollY = 0.0f;              // ピクセル単位の縦スクロール量（下端基準オフセット）
    float m_maxScrollY = 0.0f;           // 最大スクロール可能量

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture; // LOGボタンテクスチャ
};