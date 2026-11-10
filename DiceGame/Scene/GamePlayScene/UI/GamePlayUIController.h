#pragma once

#include <memory>
#include <array>
#include <string>
#include <fstream>
#include <wrl/client.h>
#include <d3d11.h>
#include <CommonStates.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <SimpleMath.h>
#include <Mouse.h>
#include <Keyboard.h>
#include <nlohmann/json.hpp>
#include "../System/DiceBattleGame.h"
#include "Framework/ObjectData.h"
#include "CameraButton.h"

// UIレイアウト用デフォルト値定数定義
namespace UIConstants
{
    constexpr float DEFAULT_DICE_CHECK_REL_X = 0.0f;
    constexpr float DEFAULT_DICE_CHECK_REL_Y = 0.0f;
    constexpr float DEFAULT_DICE_CHECK_WIDTH = 180.0f;
    constexpr float DEFAULT_DICE_CHECK_HEIGHT = 100.0f;
    constexpr float DEFAULT_DICE_CHECK_MARGIN_X = 20.0f;
    constexpr float DEFAULT_DICE_CHECK_MARGIN_Y = 20.0f;

    constexpr float DEFAULT_CAMERA_REL_X = 0.0f;
    constexpr float DEFAULT_CAMERA_REL_Y = 0.0f;
    constexpr float DEFAULT_CAMERA_WIDTH = 140.0f;
    constexpr float DEFAULT_CAMERA_HEIGHT = 60.0f;
    constexpr float DEFAULT_CAMERA_MARGIN_X = 220.0f;
    constexpr float DEFAULT_CAMERA_MARGIN_Y = 20.0f;

    constexpr float DEFAULT_LOG_REL_X = 1.0f;
    constexpr float DEFAULT_LOG_REL_Y = 0.0f;
    constexpr float DEFAULT_LOG_WIDTH = 180.0f;
    constexpr float DEFAULT_LOG_HEIGHT = 100.0f;
    constexpr float DEFAULT_LOG_MARGIN_X = -20.0f;
    constexpr float DEFAULT_LOG_MARGIN_Y = 20.0f;

    constexpr float DEFAULT_ROLL_REL_X = 1.0f;
    constexpr float DEFAULT_ROLL_REL_Y = 1.0f;
    constexpr float DEFAULT_ROLL_WIDTH = 220.0f;
    constexpr float DEFAULT_ROLL_HEIGHT = 120.0f;
    constexpr float DEFAULT_ROLL_MARGIN_X = -20.0f;
    constexpr float DEFAULT_ROLL_MARGIN_Y = -20.0f;

    constexpr float ARROW_DEFAULT_REL_X = 1.0f;
    constexpr float ARROW_DEFAULT_REL_Y = 1.0f;
    constexpr float ARROW_BUTTON_SIZE = 85.0f;
    constexpr float ARROW_SPACING = 15.0f;
}

// UIボタンの位置・サイズ設定用構造体
struct UIButtonLayout
{
    float relX = 0.0f;    // 画面横幅の比率 (0.0 = 左端, 1.0 = 右端)
    float relY = 0.0f;    // 画面縦幅の比率 (0.0 = 上端, 1.0 = 下端)
    float width = 100.0f; // ボタンの横幅 (px)
    float height = 50.0f; // ボタンの縦幅 (px)
    float marginX = 0.0f; // ピクセル補正オフセットX
    float marginY = 0.0f; // ピクセル補正オフセットY
};

// 調整対象のボタン指定（デバッグ用）
enum class UIDebugTarget
{
    None,
    DiceCheck,
    Camera,
    Log,
    RollDice
};

// ゲームプレイ中のUIボタン描画およびレイアウト管理クラス
class GamePlayUIController
{
public:
    GamePlayUIController() = default;
    ~GamePlayUIController() = default;

    // Direct3Dリソースおよびテクスチャを初期化する関数
    // 引数 device: Direct3D11デバイス, context: デバイスコンテキスト
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // 配置オブジェクトのパラメータからカメラボタンの座標領域を設定する関数
    // 引数 obj: 配置オブジェクト構造体, orthoWidth, orthoHeight: 正射影サイズ, screenWidth, screenHeight: 画面解像度
    void SetCameraBtnFromPlacedObject(const PlacedObject& obj, float orthoWidth, float orthoHeight, float screenWidth,
                                      float screenHeight)
    {
        m_cameraButton.SetFromPlacedObject(obj, orthoWidth, orthoHeight, screenWidth, screenHeight);
    }

    // ダイスチェックボタンのレイアウトを直接設定する関数
    // 引数 relX, relY: 相対位置比率, width, height: 幅高さ, marginX, marginY: オフセット
    void SetDiceCheckLayout(float relX, float relY, float width, float height, float marginX = 0.0f,
                            float marginY = 0.0f)
    {
        m_diceCheckLayout = {relX, relY, width, height, marginX, marginY};
    }

    // カメラボタンのレイアウトを直接設定する関数
    // 引数 relX, relY: 相対位置比率, width, height: 幅高さ, marginX, marginY: オフセット
    void SetCameraLayout(float relX, float relY, float width, float height, float marginX = 0.0f, float marginY = 0.0f)
    {
        m_cameraLayout = {relX, relY, width, height, marginX, marginY};
    }

    // ログボタンのレイアウトを直接設定する関数
    // 引数 relX, relY: 相対位置比率, width, height: 幅高さ, marginX, marginY: オフセット
    void SetLogButtonLayout(float relX, float relY, float width, float height, float marginX = 0.0f,
                            float marginY = 0.0f)
    {
        m_logLayout = {relX, relY, width, height, marginX, marginY};
    }

    // ダイス振りボタンのレイアウトを直接設定する関数
    // 引数 relX, relY: 相対位置比率, width, height: 幅高さ, marginX, marginY: オフセット
    void SetRollButtonLayout(float relX, float relY, float width, float height, float marginX = 0.0f,
                             float marginY = 0.0f)
    {
        m_rollLayout = {relX, relY, width, height, marginX, marginY};
    }

    // レイアウト設定をJSONファイルへ保存する関数
    // 引数 filePath: 保存先JSONファイルパス
    // 戻り値: 保存成功時 true
    bool SaveLayoutToJson(const std::string& filePath = "Resources/Data/ui_layout.json") const
    {
        nlohmann::json j;
        j["DiceCheck"] = {{"relX", m_diceCheckLayout.relX},       {"relY", m_diceCheckLayout.relY},
                          {"width", m_diceCheckLayout.width},     {"height", m_diceCheckLayout.height},
                          {"marginX", m_diceCheckLayout.marginX}, {"marginY", m_diceCheckLayout.marginY}};
        j["Camera"] = {{"relX", m_cameraLayout.relX},       {"relY", m_cameraLayout.relY},
                       {"width", m_cameraLayout.width},     {"height", m_cameraLayout.height},
                       {"marginX", m_cameraLayout.marginX}, {"marginY", m_cameraLayout.marginY}};
        j["Log"] = {{"relX", m_logLayout.relX},     {"relY", m_logLayout.relY},       {"width", m_logLayout.width},
                    {"height", m_logLayout.height}, {"marginX", m_logLayout.marginX}, {"marginY", m_logLayout.marginY}};
        j["RollDice"] = {{"relX", m_rollLayout.relX},       {"relY", m_rollLayout.relY},
                         {"width", m_rollLayout.width},     {"height", m_rollLayout.height},
                         {"marginX", m_rollLayout.marginX}, {"marginY", m_rollLayout.marginY}};

        std::ofstream file(filePath);
        if (!file.is_open())
            return false;
        file << j.dump(4);
        return true;
    }

    // レイアウト設定をJSONファイルから読み込む関数
    // 引数 filePath: 読み込み先JSONファイルパス
    // 戻り値: 読み込み成功時 true
    bool LoadLayoutFromJson(const std::string& filePath = "Resources/Data/ui_layout.json")
    {
        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        try
        {
            nlohmann::json j;
            file >> j;
            if (j.contains("DiceCheck"))
            {
                auto& d = j["DiceCheck"];
                m_diceCheckLayout = {d["relX"], d["relY"], d["width"], d["height"], d["marginX"], d["marginY"]};
            }
            if (j.contains("Camera"))
            {
                auto& c = j["Camera"];
                m_cameraLayout = {c["relX"], c["relY"], c["width"], c["height"], c["marginX"], c["marginY"]};
            }
            if (j.contains("Log"))
            {
                auto& l = j["Log"];
                m_logLayout = {l["relX"], l["relY"], l["width"], l["height"], l["marginX"], l["marginY"]};
            }
            if (j.contains("RollDice"))
            {
                auto& r = j["RollDice"];
                m_rollLayout = {r["relX"], r["relY"], r["width"], r["height"], r["marginX"], r["marginY"]};
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // デバッグキー操作によるUIボタン位置・サイズの微調整を毎フレーム更新する関数
    // 引数 kbTracker: キーボードトラッカー, kbState: キーボード状態
    void UpdateDebugAdjustment(const DirectX::Keyboard::KeyboardStateTracker& kbTracker,
                               const DirectX::Keyboard::State& kbState);

    // マウスホバー判定および動的矩形座標を更新する関数
    // 引数 mouseState: マウス状態, screenWidth, screenHeight: 画面解像度, isClient: クライアント判定フラグ,
    //      currentTurn: 現在の手番, myPlayerType: 自身のプレイヤー種別, isWaitingForRoll: ダイス待ちフラグ,
    //      remainingSteps: 残り歩数, isAnyPieceMoving: アニメーション中フラグ, isDiceViewerOpen:
    //      ビューワー表示中フラグ, isGameOver: ゲームオーバーフラグ, isLogButtonHovered: ログボタンホバーフラグ,
    //      isSelected: 選択フラグ, outLogButtonRect: 算出されたログボタン判定領域出力参照
    void Update(const DirectX::Mouse::State& mouseState, float screenWidth, float screenHeight, bool isClient,
                PlayerType currentTurn, PlayerType myPlayerType, bool isWaitingForRoll, int remainingSteps,
                bool isAnyPieceMoving, bool isDiceViewerOpen, bool isGameOver, bool isLogButtonHovered, bool isSelected,
                RECT& outLogButtonRect);

    // 各UIボタンおよび方向キー矢印を描画する関数
    // 引数 context: デバイスコンテキスト, states: コモンステート, device: Direct3D11デバイス,
    //      screenWidth, screenHeight: 画面解像度, currentTurn: 現在の手番, myPlayerType: 自身のプレイヤー種別,
    //      isWaitingForRoll: ダイス待ちフラグ, remainingSteps: 残り歩数, isDiceViewerOpen: ビューワー表示中フラグ,
    //      isGameOver: ゲームオーバーフラグ, isDraw: 引き分けフラグ, winnerPlayer: 勝者プレイヤー,
    //      turnNoticeText: 通知文字列, turnNoticeTimer: 通知タイマー, isLogButtonHovered: ログボタンホバーフラグ,
    //      logRect: ログボタン判定領域
    void Render(ID3D11DeviceContext* context, const DirectX::CommonStates& states, ID3D11Device* device,
                float screenWidth, float screenHeight, PlayerType currentTurn, PlayerType myPlayerType,
                bool isWaitingForRoll, int remainingSteps, bool isDiceViewerOpen, bool isGameOver, bool isDraw,
                PlayerType winnerPlayer, const std::wstring& turnNoticeText, float turnNoticeTimer,
                bool isLogButtonHovered, const RECT& logRect);

    // ダイス振りボタンがホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsRollButtonHovered() const
    {
        return m_isRollButtonHovered;
    }

    // ダイスチェックボタンがホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsDiceCheckBtnHovered() const
    {
        return m_isDiceCheckBtnHovered;
    }

    // ダイスチェック画面のリセットボタンがホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsDiceCheckResetHovered() const
    {
        return m_isDiceCheckResetHovered;
    }

    // ダイスチェック画面の閉じるボタンがホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsDiceCheckCloseHovered() const
    {
        return m_isDiceCheckCloseHovered;
    }

    // カメラ切り替えボタンがホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsCameraBtnHovered() const
    {
        return m_cameraButton.IsHovered();
    }

    // 上方向移動矢印がホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsUpHovered() const
    {
        return m_isUpHovered;
    }

    // 下方向移動矢印がホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsDownHovered() const
    {
        return m_isDownHovered;
    }

    // 左方向移動矢印がホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsLeftHovered() const
    {
        return m_isLeftHovered;
    }

    // 右方向移動矢印がホバー中かを取得する関数
    // 戻り値: ホバー中であれば true
    bool IsRightHovered() const
    {
        return m_isRightHovered;
    }

    // ダイス振りボタンの判定領域を取得する関数
    // 戻り値: RECT参照
    const RECT& GetRollButtonRect() const
    {
        return m_rollButtonRect;
    }

    // ダイスチェックボタンの判定領域を取得する関数
    // 戻り値: RECT参照
    const RECT& GetDiceCheckRect() const
    {
        return m_btnDiceCheckRect;
    }

    // カメラボタンの判定領域を取得する関数
    // 戻り値: RECT参照
    const RECT& GetCameraBtnRect() const
    {
        return m_cameraButton.GetRect();
    }

private:
    UIButtonLayout m_diceCheckLayout = {
        UIConstants::DEFAULT_DICE_CHECK_REL_X,    UIConstants::DEFAULT_DICE_CHECK_REL_Y,
        UIConstants::DEFAULT_DICE_CHECK_WIDTH,    UIConstants::DEFAULT_DICE_CHECK_HEIGHT,
        UIConstants::DEFAULT_DICE_CHECK_MARGIN_X, UIConstants::DEFAULT_DICE_CHECK_MARGIN_Y};
    UIButtonLayout m_cameraLayout = {UIConstants::DEFAULT_CAMERA_REL_X,    UIConstants::DEFAULT_CAMERA_REL_Y,
                                     UIConstants::DEFAULT_CAMERA_WIDTH,    UIConstants::DEFAULT_CAMERA_HEIGHT,
                                     UIConstants::DEFAULT_CAMERA_MARGIN_X, UIConstants::DEFAULT_CAMERA_MARGIN_Y};
    UIButtonLayout m_logLayout = {UIConstants::DEFAULT_LOG_REL_X,    UIConstants::DEFAULT_LOG_REL_Y,
                                  UIConstants::DEFAULT_LOG_WIDTH,    UIConstants::DEFAULT_LOG_HEIGHT,
                                  UIConstants::DEFAULT_LOG_MARGIN_X, UIConstants::DEFAULT_LOG_MARGIN_Y};
    UIButtonLayout m_rollLayout = {UIConstants::DEFAULT_ROLL_REL_X,    UIConstants::DEFAULT_ROLL_REL_Y,
                                   UIConstants::DEFAULT_ROLL_WIDTH,    UIConstants::DEFAULT_ROLL_HEIGHT,
                                   UIConstants::DEFAULT_ROLL_MARGIN_X, UIConstants::DEFAULT_ROLL_MARGIN_Y};

    UIDebugTarget m_debugTarget = UIDebugTarget::None; // デバッグ調整中のターゲットボタン

    float m_arrowRelX = UIConstants::ARROW_DEFAULT_REL_X;     // 矢印UI基準相対X
    float m_arrowRelY = UIConstants::ARROW_DEFAULT_REL_Y;     // 矢印UI基準相対Y
    float m_arrowButtonSize = UIConstants::ARROW_BUTTON_SIZE; // 矢印ボタン幅高さ
    float m_arrowSpacing = UIConstants::ARROW_SPACING;        // 矢印ボタン間隔

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rollDiceButtonTexture;  // ダイス振りボタンテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_diceCheckButtonTexture; // ダイスチェックボタンテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logButtonTexture;       // ログボタンテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowUpTexture;         // 上矢印テクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowDownTexture;       // 下矢印テクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowLeftTexture;       // 左矢印テクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_arrowRightTexture;      // 右矢印テクスチャ

    CameraButton m_cameraButton; // カメラ切り替えUIボタン

    RECT m_rollButtonRect = {0, 0, 0, 0};        // ダイス振りボタン判定領域
    RECT m_btnDiceCheckRect = {0, 0, 0, 0};      // ダイスチェックボタン判定領域
    RECT m_btnDiceCheckCloseRect = {0, 0, 0, 0}; // ダイスチェック画面閉じるボタン判定領域
    RECT m_btnDiceCheckResetRect = {0, 0, 0, 0}; // ダイスチェック画面リセットボタン判定領域

    RECT m_btnUpRect = {0, 0, 0, 0};    // 上矢印判定領域
    RECT m_btnDownRect = {0, 0, 0, 0};  // 下矢印判定領域
    RECT m_btnLeftRect = {0, 0, 0, 0};  // 左矢印判定領域
    RECT m_btnRightRect = {0, 0, 0, 0}; // 右矢印判定領域

    bool m_isRollButtonHovered = false;     // ダイス振りホバーフラグ
    bool m_isDiceCheckBtnHovered = false;   // ダイスチェックホバーフラグ
    bool m_isDiceCheckResetHovered = false; // ビューワーリセットホバーフラグ
    bool m_isDiceCheckCloseHovered = false; // ビューワー閉じるホバーフラグ

    bool m_isUpHovered = false;    // 上矢印ホバーフラグ
    bool m_isDownHovered = false;  // 下矢印ホバーフラグ
    bool m_isLeftHovered = false;  // 左矢印ホバーフラグ
    bool m_isRightHovered = false; // 右矢印ホバーフラグ

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>>
        m_primitiveBatch; // 2Dテクスチャポリゴンバッチ
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>
        m_colorPrimitiveBatch;                           // 2Dカラーポリゴンバッチ
    std::unique_ptr<DirectX::BasicEffect> m_effect;      // テクスチャ用ベーシックエフェクト
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect; // カラー用ベーシックエフェクト
};