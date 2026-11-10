#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <Effects.h>
#include <CommonStates.h>
#include <algorithm>
#include "Framework/ObjectData.h"

// カメラ切替UIボタン管理クラス
class CameraButton
{
public:
    CameraButton() = default;
    ~CameraButton() = default;

    // テクスチャおよび初期化を行う関数
    // 引数 device: Direct3D11デバイスポインタ
    void Initialize(ID3D11Device* device);

    // JSON配置オブジェクトからの座標領域設定を行う関数
    // 引数 obj: 配置オブジェクト構造体, orthoWidth, orthoHeight: 正射影幅・高さ, screenWidth, screenHeight: 画面サイズ
    void SetFromPlacedObject(const PlacedObject& obj, float orthoWidth, float orthoHeight, float screenWidth,
                             float screenHeight);

    // 外部から直接矩形領域（位置・サイズ）を設定する関数
    // 引数 rect: 設定するRECT構造体
    void SetRect(const RECT& rect)
    {
        m_rect = rect;
        m_isCustomRectSet = true;
    }

    // 毎フレームの更新（ホバー判定）を行う関数
    // 引数 mouseState: マウス状態, screenWidth, screenHeight: 画面サイズ, isDiceViewerOpen: ビューワー開閉フラグ
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight, bool isDiceViewerOpen);

    // 描画処理を行う関数
    // 引数 context: デバイスコンテキスト, primitiveBatch: 描画バッチ, effect: ベーシックエフェクト, inputLayout:
    // 入力レイアウト
    void Render(ID3D11DeviceContext* context, DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>* primitiveBatch,
                DirectX::BasicEffect* effect, ID3D11InputLayout* inputLayout);

    // ボタンにマウスがホバーしているかを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsHovered() const
    {
        return m_isHovered;
    }

    // ボタンの矩形判定領域を取得する関数
    // 戻り値: RECT参照
    const RECT& GetRect() const
    {
        return m_rect;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture; // ボタンテクスチャ参照
    RECT m_rect = {0, 0, 0, 0};                                 // ボタン判定領域
    bool m_isHovered = false;                                   // ホバー状態フラグ
    bool m_isCustomRectSet = false;                             // カスタム矩形設定済みフラグ
};