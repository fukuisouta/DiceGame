#pragma once

#include <memory>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <Keyboard.h>
#include "DiceViewer.h"

namespace Imase
{
    class DebugRenderer;
}

class Dice;
class GamePlayUIController;

// ダイスチェック画面（ビューワー）のモーダル表示・描画および開閉制御クラス
class DiceViewerController
{
public:
    DiceViewerController() = default;
    ~DiceViewerController() = default;

    // 初期化処理を行う関数
    // 引数 device: Direct3D11デバイス, context: デバイスコンテキスト, aspectRatio: 画面アスペクト比
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float aspectRatio);

    // ダイスチェック画面の更新（開閉・ボタン押下・カメラ操作）を行う関数
    // 引数 dt: デルタタイム, mouseState: マウス状態, mouseTracker: マウストラッカー, kbTracker: キーボードトラッカー,
    //      uiController: UIコントローラー参照, targetDice: 対象のダイスモデルポインタ, screenWidth, screenHeight:
    //      画面サイズ
    // 戻り値: ビューワー制御中・処理済みの場合は true
    bool Update(float dt, const DirectX::Mouse::State& mouseState,
                const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                const DirectX::Keyboard::KeyboardStateTracker& kbTracker, const GamePlayUIController& uiController,
                Dice* targetDice, float screenWidth, float screenHeight);

    // ダイスチェック画面および対象3Dダイスモデルの描画を行う関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, debugRenderer: デバッグレンダラー参照,
    //      uiController: UIコントローラー参照, targetDice: 対象のダイスモデルポインタ, screenWidth, screenHeight:
    //      画面サイズ
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                const GamePlayUIController& uiController, Dice* targetDice, float screenWidth, float screenHeight);

    // ダイスチェック画面を開く関数
    // 引数 targetDice: 対象ダイスポインタ
    void Open(Dice* targetDice = nullptr);

    // ダイスチェック画面を閉じる関数
    void Close()
    {
        m_diceViewer.Close();
    }

    // ダイスチェック画面が開かれているかを取得する関数
    // 戻り値: 開いている場合 true
    bool IsOpen() const
    {
        return m_diceViewer.IsOpen();
    }

private:
    DiceViewer m_diceViewer; // 内部ビューワーモデル
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch; // 2Dカラーバッチ
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;                                          // カラーエフェクト
};