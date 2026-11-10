#pragma once

#include <array>
#include <memory>
#include <vector>
#include <string>
#include <cwchar>
#include <cstdarg>
#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <WICTextureLoader.h>
#include <GeometricPrimitive.h>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "DiceBattleGame.h"
#include "DiceBattleAI.h"
#include "GridField.h"
#include "PieceDropAnimator.h"
#include "Framework/ObjectData.h"
#include "Framework/SoundManager.h"

#include "GameLogManager.h"
#include "ParticleManager.h"
#include "MovableTileCalculator.h"
#include "CameraManager.h"
#include "GamePlayUIController.h"
#include "GameJudgeManager.h"
#include "PieceMovementController.h"
#include "TurnManager.h"
#include "DiceViewerController.h"
#include "Dice/KnockbackDice.h"

class Dice;

// ゲームプレイ共通定数定義
namespace GamePlayBaseConstants
{
    constexpr float DEFAULT_GRID_CELL_SIZE = 2.0f;         // グリッド1マスのサイズ
    constexpr float DEFAULT_AI_THINK_DELAY = 1.0f;         // AIの思考ディレイ時間（秒）
    constexpr float DEFAULT_NOTICE_DURATION = 1.5f;        // ターン通知の標準表示時間（秒）
    constexpr float GAME_START_NOTICE_DURATION = 1.8f;     // ゲーム開始通知の表示時間（秒）
    constexpr float KNOCKBACK_DURATION = 0.8f;             // 撃破ダイス吹っ飛びアニメーション時間（秒）
    constexpr int DEFEAT_PARTICLE_COUNT = 30;              // 被弾時の発生エフェクト粒子数
    constexpr int LOG_BUTTON_CLICK_PARTICLE_COUNT = 20;    // ログボタンクリック時の粒子数
    constexpr int CAMERA_BUTTON_CLICK_PARTICLE_COUNT = 20; // カメラ切り替え時の粒子数
    constexpr int MOUSE_CLICK_PARTICLE_COUNT = 25;         // マウスクリック時の粒子数
    constexpr int DICE_FACE_COUNT = 6;                     // ダイスの面数
    constexpr size_t LOG_BUFFER_SIZE = 512;                // ログフォーマット用バッファサイズ

    constexpr float FOV_ANGLE_Y = 45.0f; // 画角 (度)
    constexpr float NEAR_PLANE = 0.1f;   // 近平面
    constexpr float FAR_PLANE = 1000.0f; // 遠平面

    constexpr float SELECTION_MARKER_SIZE = 0.8f;     // 選択マーカーの底面サイズ
    constexpr float SELECTION_MARKER_HEIGHT = 1.0f;   // 選択マーカーの高さサイズ
    constexpr int SELECTION_MARKER_TESSELLATION = 12; // 選択マーカーの分割数

    constexpr float NOTICE_TEXT_X_RATIO = 0.5f;    // ターン通知表示位置X割合
    constexpr float NOTICE_TEXT_Y_RATIO = 0.35f;   // ターン通知表示位置Y割合
    constexpr float NOTICE_TEXT_CHAR_WIDTH = 8.0f; // ターン通知テキスト1文字幅
}

// ゲームプレイシーンの共通基底クラス
class GamePlaySceneBase : public Imase::SceneBase<SceneId, GameContext>
{
public:
    GamePlaySceneBase();
    virtual ~GamePlaySceneBase();

    // シーン遷移時に呼び出され、盤面や対戦システムの初期化を行う関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void OnEnter(GameContext& gameContext) override;

    // フレーム毎のターン状態、カメラ、入力、描画オブジェクト更新を行う関数
    // 引数 sceneController: シーン遷移管理コントローラー, gameContext: ゲーム全体の共有コンテキスト
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 盤面、3Dダイス、吹っ飛び演出、2D UI、ゲーム判定ダイアログを描画する関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void Render(GameContext& gameContext) override;

    // 画面中央にターン交代やイベント通知のポップアップを表示する関数
    // 引数 text: 表示メッセージ文字列, duration: 表示時間（秒）
    void TriggerTurnNotice(const std::wstring& text, float duration = GamePlayBaseConstants::DEFAULT_NOTICE_DURATION);

    // コマの選択・移動操作状態をリセットする関数
    void ResetMoveState();

    // フォーマット付きで行動ログウィンドウにテキストを追加する関数
    // 引数 fmt: 書式指定文字列, ...: 可変長引数
    void AddLog(const wchar_t* fmt, ...)
    {
        wchar_t buffer[GamePlayBaseConstants::LOG_BUFFER_SIZE];
        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer, _countof(buffer), fmt, args);
        va_end(args);
        m_logManager.AddLog(buffer);
    }

    // 撃破されたダイスの吹っ飛びアニメーションインスタンスを生成して追記する関数
    // 引数 device: Direct3D11デバイス, context: Direct3D11デバイスコンテキスト, attackerPos: 攻撃側座標, victimPos:
    // 被弾側座標, modelPath: ダイスモデルのパス
    void SpawnKnockbackDice(ID3D11Device* device, ID3D11DeviceContext* context,
                            const DirectX::SimpleMath::Vector3& attackerPos,
                            const DirectX::SimpleMath::Vector3& victimPos,
                            const std::string& modelPath = "Resources/Models/Dice.cmo")
    {
        auto kbDice = std::make_unique<KnockbackDice>();
        kbDice->Initialize(device, context, modelPath);
        kbDice->TriggerKnockback(attackerPos, victimPos, GamePlayBaseConstants::KNOCKBACK_DURATION);
        m_knockbackDiceList.push_back(std::move(kbDice));
    }

protected:
    // モード固有の初期化処理を実行する抽象関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    virtual void OnModeEnter(GameContext& gameContext) = 0;

    // モード固有のロジック・スキル更新処理を実行する抽象関数
    // 引数 dt: デルタタイム, gameContext: ゲーム全体の共有コンテキスト
    virtual void OnModeUpdate(float dt, GameContext& gameContext) = 0;

    // モード固有の2D UI描画処理を実行する抽象関数
    // 引数 context: デバイスコンテキスト, screenWidth: 画面幅, screenHeight: 画面高さ, gameContext: 共有コンテキスト
    virtual void OnModeRender2D(ID3D11DeviceContext* context, float screenWidth, float screenHeight,
                                GameContext& gameContext) = 0;

    // 蘇生ターゲットなどの選択モード中かどうかを判定する抽象関数
    // 戻り値: 選択中であれば true, 通常時であれば false
    virtual bool IsModeSelectingTarget() const = 0;

    // マス目選択モード時のクリック判定をハンドリングする抽象関数
    // 引数 gridX: クリックされたX格子座標, gridY: クリックされたY格子座標
    virtual void OnModeGridClicked(int gridX, int gridY) = 0;

    // JSONファイルからステージモデルや配置データをロードする内部関数
    // 引数 gameContext: 共有コンテキスト, filePath: JSONファイルパス
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

    // 現在プレイヤーが選択中またはフォーカスしているダイスのインスタンスを取得する内部関数
    // 戻り値: 該当するDiceポインタ
    Dice* GetTargetDiceInstance();

    // 選択されたコマから移動可能なマス情報の一覧を算出する内部関数
    // 戻り値: 移動可能マス情報の配列
    std::vector<MovableTileInfo> GetMovableTiles() const;

protected:
    GridField m_field;          // 盤面グリッド描画オブジェクト
    DiceBattleGame m_gameLogic; // ダイスバトルルール・ロジック対戦システム

    std::vector<std::unique_ptr<KnockbackDice>> m_knockbackDiceList; // 吹っ飛びアニメーション演出中のダイスリスト

    DiceBattleAI m_ai;                 // CPU対戦用AI思考システム
    PieceDropAnimator m_dropAnimator;  // 対戦開始時のコマ落下アニメーション演出
    ParticleManager m_particleManager; // マウスクリックや被弾時の粒子エフェクト
    GameLogManager m_logManager;       // 行動ログ表示ウィンドウUI
    CameraManager m_cameraManager;     // 対戦視点カメラ制御

    GamePlayUIController m_uiController; // ゲーム中のボタン・勝敗演出UI制御

    std::unique_ptr<GameJudgeManager> m_judgeManager; // ゲーム勝敗・リトライダイアログ管理

    PieceMovementController m_movementController; // コマ選択・格子移動ロジック
    TurnManager m_turnManager;                    // ターン切り替え通知・タイミング管理
    DiceViewerController m_diceViewerController;  // 選択ダイスの3D詳細出目確認画面

    std::vector<PlacedObject> m_placedObjects; // シーンファイルからロードしたオブジェクト定義
    std::vector<PlacedObject> m_loadedObjects; // 実際の実体モデルオブジェクトリスト

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch; // 2Dテクスチャ描画バッチ
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>
        m_colorPrimitiveBatch;                           // 2D単色ポリゴン描画バッチ
    std::unique_ptr<DirectX::BasicEffect> m_effect;      // 2Dテクスチャエフェクト
    std::unique_ptr<DirectX::BasicEffect> m_colorEffect; // 2Dカラーエフェクト
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 7>
        m_diceCountTextures; // 出目（1〜6）用数字テクスチャ配列

    std::unique_ptr<DirectX::GeometricPrimitive> m_selectionMarker; // マス選択中マーカー描画用コーン
    float m_markerAnimTimer = 0.0f;                                 // マーカーアニメーション時間タイマー
};