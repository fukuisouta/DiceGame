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

// ============================================================================
// ダイスチェック画面（ビューワー）のモーダル表示・描画および開閉制御クラス
// ============================================================================
class DiceViewerController
{
public:
    DiceViewerController() = default;
    ~DiceViewerController() = default;

    // 初期化処理
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float aspectRatio);

    // ダイスチェック画面の更新（開閉・ボタン押下・カメラ操作）
    bool Update(float dt, const DirectX::Mouse::State& mouseState,
                const DirectX::Mouse::ButtonStateTracker& mouseTracker,
                const DirectX::Keyboard::KeyboardStateTracker& kbTracker, const GamePlayUIController& uiController,
                Dice* targetDice, float screenWidth, float screenHeight);

    // ダイスチェック画面および対象3Dダイスモデルの描画
    void Render(ID3D11DeviceContext* context, DirectX::CommonStates& states, Imase::DebugRenderer& debugRenderer,
                const GamePlayUIController& uiController, Dice* targetDice, float screenWidth, float screenHeight);

    // 開閉制御
    void Open(Dice* targetDice = nullptr);
    void Close()
    {
        m_diceViewer.Close();
    }
    bool IsOpen() const
    {
        return m_diceViewer.IsOpen();
    }

private:
    DiceViewer m_diceViewer; // 内部ビューワー
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
};