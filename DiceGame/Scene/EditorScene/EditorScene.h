#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <d3d11.h>
#include <SimpleMath.h>
#include <Model.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <wrl/client.h>
#include <WICTextureLoader.h>

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "GridField.h"
#include "DiceBattleGame.h"
#include "Framework/ObjectData.h"

// エディタシーン定数定義
namespace EditorConstants
{
    constexpr int MODE_GRID = 0;          // 盤面グリッド編集モード
    constexpr int MODE_SCENE_OBJECTS = 1; // 3D/2Dオブジェクト配置編集モード

    constexpr float DEFAULT_GRID_CELL_SIZE = 2.0f; // 盤面グリッドの1マスサイズ

    constexpr float DEFAULT_CAMERA_MOVE_SPEED = 15.0f;    // 通常時のカメラ移動スピード
    constexpr float FAST_CAMERA_MOVE_SPEED_SCALE = 2.5f;  // Shiftキー押下時の移動速度倍率
    constexpr float DEFAULT_CAMERA_ROTATE_SPEED = 0.005f; // マウスドラッグ時のカメラ回転感度
    constexpr float MOUSE_WHEEL_ZOOM_SCALE = 2.0f;        // マウスホイールによる前後移動感度

    constexpr float CAMERA_PITCH_LIMIT_MARGIN = 0.1f; // カメラのピッチ角回転制限マージン (ラジアン)

    constexpr float FOV_ANGLE_Y = 45.0f;  // 3D透視投影カメラの画角 (度数法)
    constexpr float NEAR_PLANE = 0.1f;    // 近平面クリップ距離
    constexpr float FAR_PLANE = 1000.0f;  // 遠平面クリップ距離
    constexpr float ORTHO_HEIGHT = 10.0f; // 2D平行投影時の縦幅サイズ

    constexpr float DUP_OFFSET_X = 0.5f;  // 複製・貼り付け時のX座標ずらしオフセット
    constexpr float DUP_OFFSET_Y = -0.5f; // 複製・貼り付け時のY座標ずらしオフセット

    constexpr float RAY_EPSILON = 0.0001f;  // ゼロ除算回避用微小値
    constexpr float AABB_HALF_SCALE = 0.5f; // AABB判定用バウンディングボックス半サイズ倍率

    constexpr size_t TAG_BUFFER_SIZE = 64; // UI用タグ入力文字列バッファサイズ
}

// レベルエディター・シーンオブジェクト配置編集クラス
class EditorScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    EditorScene() = default;
    ~EditorScene() override = default;

    // シーン切り替え時に呼び出され、描画リソースや初期データをロードする関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void OnEnter(GameContext& gameContext) override;

    // フレーム毎のカメラ操作、マウスクリック選択、キー入力を更新する関数
    // 引数 sceneController: シーン遷移管理コントローラー, gameContext: ゲーム全体の共有コンテキスト
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;

    // 3Dモデル、2D UI要素、ImGuiパラメータ調整ウィンドウを描画する関数
    // 引数 gameContext: ゲーム全体の共有コンテキスト
    void Render(GameContext& gameContext) override;

    // 外部ファイル（.cmo / .png / .json）のドラッグ＆ドロップを受け取って読み込む関数
    // 引数 gameContext: ゲーム全体コンテキスト, fullPath: ドロップされたファイルの絶対パス
    void OnFileDropped(GameContext& gameContext, const std::wstring& fullPath) override;

private:
    // JSONファイルからシーンのオブジェクト配置データを読み込む内部関数
    // 引数 gameContext: ゲーム全体コンテキスト, filePath: 読み込み対象ファイルパス
    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);

    // 現在配置されている全オブジェクトデータをJSONファイルへ書き出す内部関数
    // 引数 filePath: 出力先ファイルパス
    void SaveSceneToJson(const std::string& filePath);

    // 視線レイとAABB（軸平行境界ボックス）の交差判定を行い、マウスクリック選択（ピッキング）を行う関数
    // 引数 rayOrigin: レイの始点, rayDir: レイの方向, boxMin: AABBの最小座標, boxMax: AABBの最大座標, outT:
    // 交差距離の出力参照 戻り値: 衝突している場合 true, 衝突していない場合 false
    bool RayIntersectsAABB(const DirectX::SimpleMath::Vector3& rayOrigin, const DirectX::SimpleMath::Vector3& rayDir,
                           const DirectX::SimpleMath::Vector3& boxMin, const DirectX::SimpleMath::Vector3& boxMax,
                           float& outT);

    // テクスチャリソースの読み込み、および読み込み済みキャッシュを取得する関数
    // 引数 device: Direct3D11デバイス, texName: テクスチャファイル名
    // 戻り値: 生成または取得されたシェーダーリソースビュー
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrLoadTexture(ID3D11Device* device, const std::string& texName);

    // 2D UI要素描画用の板ポリゴン（クアッド）をPrimitiveBatchで描画する関数
    // 引数 context: Direct3D11デバイスコンテキスト
    void DrawQuadWithPrimitiveBatch(ID3D11DeviceContext* context);

    // 現在選択されているオブジェクトのデータをクリップボードへ複製保存する関数
    void CopySelectedObject();

    // クリップボードに保持されているオブジェクトをシーンへ追加配置する関数
    void PasteObject();

    // 指定インデックスのオブジェクトを複製して新しいオブジェクトとして追加する関数
    // 引数 index: 複製対象のオブジェクトインデックス
    void DuplicateObject(int index);

private:
    GridField m_field;          // 盤面グリッド描画オブジェクト
    DiceBattleGame m_gameLogic; // ダイスバトルのゲームルール・ロジック参照

    int m_editorMode = EditorConstants::MODE_SCENE_OBJECTS; // 編集モード (0: 盤面編集, 1: オブジェクト配置編集)
    bool m_isPlaying = false;             // エディタ上での対戦動作・ボタン挙動プレビューテスト実行フラグ
    bool m_transitionToPlayScene = false; // ゲーム本編シーンへの遷移要求フラグ

    std::string m_currentFilePath =
        "Resources/Data/title_scene.json"; // 現在編集・保存対象となっているJSONファイルのパス

    std::vector<PlacedObject> m_placedObjects; // シーン内に配置されている全オブジェクトの配列
    int m_selectedIndex = -1;                  // 現在選択中のオブジェクト配列インデックス (-1は未選択)

    PlacedObject m_clipboardObject; // コピー＆ペースト用に一時保持しているオブジェクトデータ
    bool m_hasClipboard = false;    // クリップボードにデータが保持されているかを示すフラグ

    DirectX::SimpleMath::Vector3 m_sceneRootPosition = {0.0f, 0.0f, 0.0f}; // シーン全体のルート位置オフセット
    DirectX::SimpleMath::Vector3 m_sceneRootRotation = {0.0f, 0.0f, 0.0f}; // シーン全体のルート回転オフセット

    DirectX::SimpleMath::Vector3 m_cameraPos = {0.0f, 0.0f, -10.0f};          // フリーカメラのワールド位置座標
    float m_cameraYaw = 0.0f;                                                 // カメラの水平回転角 (ヨー)
    float m_cameraPitch = 0.0f;                                               // カメラの垂直回転角 (ピッチ)
    float m_cameraMoveSpeed = EditorConstants::DEFAULT_CAMERA_MOVE_SPEED;     // カメラの標準移動スピード
    float m_cameraRotateSpeed = EditorConstants::DEFAULT_CAMERA_ROTATE_SPEED; // マウス操作によるカメラ回転感度

    DirectX::SimpleMath::Matrix m_view;       // 算出した現在のビュー変換行列
    DirectX::SimpleMath::Matrix m_projection; // 算出した現在のプロジェクション変換行列

    std::unique_ptr<DirectX::EffectFactory> m_fxFactory; // CMOモデル読み込み用エフェクトファクトリ

    std::unique_ptr<DirectX::BasicEffect> m_effect;          // 2D UI描画用ベーシックエフェクト
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout; // 2D UI用入力レイアウト
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionNormalTexture>>
        m_primitiveBatch; // 2D板ポリゴン描画バッチ
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_textureCache; // テクスチャ読み込みキャッシュマップ

    RECT m_startBtnRect = {0, 0, 0, 0}; // プレビューテスト用スタートボタンのスクリーン領域 (RECT)
    RECT m_exitBtnRect = {0, 0, 0, 0};  // プレビューテスト用終了ボタンのスクリーン領域 (RECT)
    bool m_isStartHovered = false;      // プレビューテスト時にスタートボタン上にカーソルがあるかを示すフラグ
    bool m_isExitHovered = false;       // プレビューテスト時に終了ボタン上にカーソルがあるかを示すフラグ
};