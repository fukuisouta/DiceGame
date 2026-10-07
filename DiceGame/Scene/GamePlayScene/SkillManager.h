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

enum class SkillType
{
    //Destroy,
    ReRoll,
    Shield,
    Revive,
    //Earthquake,
    Freeze,
    DoubleRoll
};

struct SkillCard
{
    SkillType type;
    std::wstring name;
    std::wstring description;
    RECT rect; // 描画領域 (Left, Top, Right, Bottom)
    bool isHovered = false;
    bool isUsed = false;
    DirectX::SimpleMath::Vector4 color = DirectX::SimpleMath::Vector4(1, 1, 1, 1);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
};

class SkillManager
{
public:
    SkillManager() = default;
    ~SkillManager() = default;

    void Initialize(ID3D11Device* device);
    void InitializeHandCards();

    void ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                 bool hasMovedThisTurn);
    //void ExecuteDestroySkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic);
    void ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic);
    //void ExecuteEarthquakeSkill(GamePlaySceneBase* scene, DiceBattleGame& gameLogic);
    void ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic, bool hasMovedThisTurn);

    // ゲッター / セッター
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

    //bool IsDestroySelecting() const
    //{
    //    return m_isDestroySelecting;
    //}
    //void SetDestroySelecting(bool val)
    //{
    //    m_isDestroySelecting = val;
    //}

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
    std::array<SkillCard, 3> m_handCards;
    std::array<SkillCard, 3> m_aiHandCards;
    std::unordered_map<SkillType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_cardTextures;

    //bool m_isDestroySelecting = false;
    bool m_isReviveSelecting = false;
    bool m_isFreezeNextTurn = false;
    bool m_isDoubleRollActive = false;

    float m_aiSkillTimer = 0.0f;
};