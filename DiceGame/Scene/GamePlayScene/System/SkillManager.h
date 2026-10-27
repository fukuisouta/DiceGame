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

// スキルカードの種類定義
enum class SkillType
{
    // Destroy,    // 破壊（コメントアウト中）
    ReRoll, // 振り直し
    Shield, // 守護（コマ保護）
    Revive, // 蘇生（自陣復活）
            // Earthquake, // 地震（コメントアウト中）
    Freeze,    // 凍結（相手のターン立ち飛ばし）
    DoubleRoll // 倍化（移動数2倍）
};

// 手札スキルカードのデータ構造体
struct SkillCard
{
    SkillType type;
    std::wstring name;
    std::wstring description;
    RECT rect; // 画面上の描画・判定領域 (Left, Top, Right, Bottom)
    bool isHovered = false;
    bool isUsed = false;
    DirectX::SimpleMath::Vector4 color = DirectX::SimpleMath::Vector4(1, 1, 1, 1);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
};

// ============================================================================
// スキルカードのデッキ管理、手札配布、効果発動およびAIのスキル思考制御クラス
// ============================================================================
class SkillManager
{
public:
    SkillManager() = default;
    ~SkillManager() = default;

    // スキルカードテクスチャの読み込みと初期化
    void Initialize(ID3D11Device* device);
    // プレイヤーおよびAI用の手札カードのランダム配布
    void InitializeHandCards();

    // プレイヤーがスキルカードをクリックした際の発動・選択モード処理
    void ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                 bool hasMovedThisTurn);
    // void ExecuteDestroySkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic);

    // 蘇生スキルの実行（指定マスにコマを復帰）
    void ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic);
    // void ExecuteEarthquakeSkill(GamePlaySceneBase* scene, DiceBattleGame& gameLogic);

    // AI（CPU）の手札状況に応じたスキル使用判定ロジック
    void ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic, bool hasMovedThisTurn);

    // --- ゲッター / セッター ---
    std::array<SkillCard, 3>& GetHandCards()
    {
        return m_handCards;
    }
    const std::array<SkillCard, 3>& GetHandCards() const
    {
        return m_handCards;
    }

    std::array<SkillCard, 3>& GetAIHandCards()
    {
        return m_aiHandCards;
    }

    // bool IsDestroySelecting() const
    //{
    //     return m_isDestroySelecting;
    // }
    // void SetDestroySelecting(bool val)
    //{
    //     m_isDestroySelecting = val;
    // }

    bool IsReviveSelecting() const
    {
        return m_isReviveSelecting;
    }
    void SetReviveSelecting(bool val)
    {
        m_isReviveSelecting = val;
    }

    bool IsFreezeNextTurn() const
    {
        return m_isFreezeNextTurn;
    }
    void SetFreezeNextTurn(bool val)
    {
        m_isFreezeNextTurn = val;
    }

    bool IsDoubleRollActive() const
    {
        return m_isDoubleRollActive;
    }
    void SetDoubleRollActive(bool val)
    {
        m_isDoubleRollActive = val;
    }

private:
    std::array<SkillCard, 3> m_handCards;   // プレイヤー（1P）の手札カード（最大3枚）
    std::array<SkillCard, 3> m_aiHandCards; // CPU（2P）の手札カード（最大3枚）
    std::unordered_map<SkillType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_cardTextures;

    // bool m_isDestroySelecting = false;
    bool m_isReviveSelecting = false;  // 蘇生位置のターゲット選択中フラグ
    bool m_isFreezeNextTurn = false;   // 次ターン凍結（ターンスキップ）予約フラグ
    bool m_isDoubleRollActive = false; // ダブルロール（移動数2倍）発動中フラグ

    float m_aiSkillTimer = 0.0f; // AIのスキル使用思考用タイマー
};