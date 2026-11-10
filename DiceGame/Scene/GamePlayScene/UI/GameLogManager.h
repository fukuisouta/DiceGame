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

// 対戦行動ログの蓄積・自動折り返し改行・スクロール表示およびUIボタン管理クラス
class GameLogManager
{
public:
    GameLogManager() = default;
    ~GameLogManager() = default;

    // リソース・テクスチャの初期化を行う関数
    // 引数 device: Direct3D11デバイスポインタ, context: デバイスコンテキストポインタ
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ログメッセージの追加（自動折り返し処理付き）を行う関数
    // 引数 message: 追加するメッセージ文字列
    void AddLog(const std::wstring& message);

    // フォーマット付きログメッセージを追加する関数
    // 引数 fmt: フォーマット文字列
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
    // 引数 mouseState: マウス状態, screenWidth: 画面幅, screenHeight: 画面高さ
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight);

    // ログ表示ウィンドウの描画を行う関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, debugRenderer: デバッグレンダラー参照,
    //      batchColor: カラーポリゴン描画バッチ, effectColor: カラーエフェクト, screenWidth: 画面幅, screenHeight:
    //      画面高さ
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* batchColor, DirectX::BasicEffect* effectColor,
                float screenWidth, float screenHeight);

    // LOG表示切替ボタン（LOG.png）の描画を行う関数
    // 引数 context: デバイスコンテキスト, batch: テクスチャポリゴン描画バッチ, effect: ベーシックエフェクト,
    // inputLayout: 入力レイアウト
    void RenderButton(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* batch,
                      DirectX::BasicEffect* effect, ID3D11InputLayout* inputLayout);

    // ウィンドウ開閉状態を切り替える関数
    void ToggleWindow();

    // ボタンホバー状態を取得する関数
    // 戻り値: ホバー中であれば true
    bool IsButtonHovered() const
    {
        return m_isButtonHovered;
    }

    // ボタンクリック判定を取得する関数
    // 引数 tracker: マウストラッカー
    // 戻り値: クリックされた場合 true
    bool IsButtonClicked(DirectX::Mouse::ButtonStateTracker& tracker) const;

    // ウィンドウが表示中かを取得する関数
    // 戻り値: 表示中であれば true
    bool IsWindowVisible() const
    {
        return m_showLogWindow;
    }

    // ボタン矩形領域を取得する関数
    // 戻り値: ボタンのRECT参照
    const RECT& GetButtonRect() const
    {
        return m_logButtonRect;
    }

    // ボタン矩形領域を設定する関数
    // 引数 rect: 設定するRECT
    void SetButtonRect(const RECT& rect)
    {
        m_logButtonRect = rect;
    }

private:
    std::vector<std::wstring> m_logs;    // ログテキスト履歴
    bool m_showLogWindow = false;        // ウィンドウ表示フラグ
    bool m_isButtonHovered = false;      // ボタンホバーフラグ
    RECT m_logButtonRect = {0, 0, 0, 0}; // ボタン判定領域
    int m_scrollOffset = 0;              // スクロール位置

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture; // LOGボタンテクスチャ
};