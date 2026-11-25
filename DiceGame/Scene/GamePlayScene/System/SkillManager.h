#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "DiceBattleGame.h"

class GamePlaySceneBase;

namespace SkillManagerConstants
{
    constexpr size_t HAND_CARD_COUNT = 3;
}

// スキルカードの種類定義（凍結を除外）
enum class SkillType
{
    ReRoll,    // 振り直し
    Shield,    // 守護（コマ保護）
    Revive,    // 蘇生（自陣復活）
    DoubleRoll // 倍化（移動数2倍）
};

// 手札スキルカードのデータ構造体
struct SkillCard
{
    SkillType type;
    std::wstring name;
    std::wstring description;
    RECT rect;
    bool isHovered = false;
    bool isUsed = false;
    DirectX::SimpleMath::Vector4 color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
};

// スキルカードのデッキ管理、手札配布、効果発動制御クラス
class SkillManager
{
public:
    SkillManager() = default;
    ~SkillManager() = default;

    // スキルカードテクスチャの読み込みと初期化を行う関数
    void Initialize(ID3D11Device* device);

    // プレイヤーおよびAI用の手札カードのランダム配布を行う関数
    void InitializeHandCards();

    // プレイヤーがスキルカードをクリックした際の発動・選択モード処理を行う関数
    void ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                 bool hasMovedThisTurn, PlayerType myPlayerType);

    // 通信相手から受信したスキルカード発動命令を自ゲーム画面へ反映適用する関数
    void ProcessRemoteSkillClick(SkillType skillType, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                 bool hasMovedThisTurn, PlayerType enemyPlayerType, int targetX = -1, int targetY = -1);

    // 蘇生スキルを実行し、対象プレイヤーの初期陣地へコマを復帰させる関数
    void ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                            PlayerType myPlayerType);

    // CPU（2P）の手札状況に応じたスキル使用判定ロジックを実行する関数
    void ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic, bool hasMovedThisTurn);

    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetHandCards()
    {
        return m_handCards;
    }

    const std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetHandCards() const
    {
        return m_handCards;
    }

    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT>& GetAIHandCards()
    {
        return m_aiHandCards;
    }

    bool IsReviveSelecting() const
    {
        return m_isReviveSelecting;
    }

    void SetReviveSelecting(bool val)
    {
        m_isReviveSelecting = val;
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
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT> m_handCards;
    std::array<SkillCard, SkillManagerConstants::HAND_CARD_COUNT> m_aiHandCards;
    std::unordered_map<SkillType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_cardTextures;

    bool m_isReviveSelecting = false;
    bool m_isDoubleRollActive = false;

    float m_aiSkillTimer = 0.0f;
};