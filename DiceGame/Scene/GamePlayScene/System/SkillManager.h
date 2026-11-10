#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

// 前方宣言
class GamePlaySceneBase;
class DiceBattleGame;

// スキルマネージャー用定数定義
namespace SkillManagerConstants
{
    constexpr size_t HAND_CARD_COUNT = 3; // 手札の最大カード所持枚数
}

// スキルカードの種類定義
enum class SkillType
{
    ReRoll,    // 振り直し
    Shield,    // 守護（コマ保護）
    Revive,    // 蘇生（自陣復活）
    Freeze,    // 凍結（相手のターン立ち飛ばし）
    DoubleRoll // 倍化（移動数2倍）
};

// 手札スキルカードのデータ構造体
struct SkillCard
{
    SkillType type;           // スキルの種別
    std::wstring name;        // カード表示名
    std::wstring description; // カード効果説明テキスト
    RECT rect;                // 画面上の描画・判定領域 (Left, Top, Right, Bottom)
    bool isHovered = false;   // マウスホバー状態フラグ
    bool isUsed = false;      // 使用済みフラグ
    DirectX::SimpleMath::Vector4 color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 描画カラー
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;                                  // カードテクスチャ参照
};

// スキルカードのデッキ管理、手札配布、効果発動およびAIのスキル思考制御クラス
class SkillManager
{
public:
    SkillManager() = default;
    ~SkillManager() = default;

    // スキルカードテクスチャの読み込みと初期化を行う関数
    // 引数 device: Direct3D11デバイスポインタ
    void Initialize(ID3D11Device* device);

    // プレイヤーおよびAI用の手札カードのランダム配布を行う関数
    void InitializeHandCards();

    // プレイヤーがスキルカードをクリックした際の発動・選択モード処理を行う関数
    // 引数 card: 対象のスキルカード参照, scene: ゲームプレイシーンポインタ, gameLogic: ゲームロジック参照,
    // hasMovedThisTurn: このターン移動済みフラグ
    void ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                 bool hasMovedThisTurn);

    // 蘇生スキルを実行（指定マスにコマを復帰）する関数
    // 引数 targetX, targetY: 復帰対象の格子座標, scene: ゲームプレイシーンポインタ, gameLogic: ゲームロジック参照
    void ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic);

    // AI（CPU）の手札状況に応じたスキル使用判定ロジックを実行する関数
    // 引数 dt: デルタタイム, scene: ゲームプレイシーンポインタ, gameLogic: ゲームロジック参照, hasMovedThisTurn:
    // このターン移動済みフラグ
    void ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic, bool hasMovedThisTurn);

    // プレイヤー（1P）の手札カード配列を取得する関数
    // 戻り値: プレイヤーの手札カード配列参照
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetHandCards()
    {
        return m_handCards;
    }

    // プレイヤー（1P）の手札カード配列を取得する関数（const版）
    // 戻り値: プレイヤーの手札カード配列のconst参照
    const std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetHandCards() const
    {
        return m_handCards;
    }

    // CPU（2P）の手札カード配列を取得する関数
    // 戻り値: CPUの手札カード配列参照
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetAIHandCards()
    {
        return m_aiHandCards;
    }

    // 蘇生ターゲット選択中かどうかを取得する関数
    // 戻り値: 選択中であれば true
    bool IsReviveSelecting() const
    {
        return m_isReviveSelecting;
    }

    // 蘇生ターゲット選択中フラグを設定する関数
    // 引数 val: 設定フラグ値
    void SetReviveSelecting(bool val)
    {
        m_isReviveSelecting = val;
    }

    // 次ターン凍結（ターンスキップ）が予約されているかを取得する関数
    // 戻り値: 予約されていれば true
    bool IsFreezeNextTurn() const
    {
        return m_isFreezeNextTurn;
    }

    // 次ターン凍結予約フラグを設定する関数
    // 引数 val: 設定フラグ値
    void SetFreezeNextTurn(bool val)
    {
        m_isFreezeNextTurn = val;
    }

    // ダブルロール（移動数2倍）効果が発動中かを取得する関数
    // 戻り値: 発動中であれば true
    bool IsDoubleRollActive() const
    {
        return m_isDoubleRollActive;
    }

    // ダブルロール発動フラグを設定する関数
    // 引数 val: 設定フラグ値
    void SetDoubleRollActive(bool val)
    {
        m_isDoubleRollActive = val;
    }

private:
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT> m_handCards;   // プレイヤー（1P）の手札カード配列
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT> m_aiHandCards; // CPU（2P）の手札カード配列
    std::unordered_map<SkillType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
        m_cardTextures; // 各スキルのカードテクスチャマップ

    bool m_isReviveSelecting = false;  // 蘇生位置のターゲット選択中フラグ
    bool m_isFreezeNextTurn = false;   // 次ターン凍結（ターンスキップ）予約フラグ
    bool m_isDoubleRollActive = false; // ダブルロール（移動数2倍）発動中フラグ

    float m_aiSkillTimer = 0.0f; // AIのスキル使用思考用タイマー
};