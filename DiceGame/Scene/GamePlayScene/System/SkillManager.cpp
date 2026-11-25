#include "pch.h"
#include "SkillManager.h"
#include "../Base/GamePlaySceneBase.h"
#include <WICTextureLoader.h>
#include <algorithm>
#include <random>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace SkillStyleConstants
{
    constexpr float AI_SKILL_THINK_INTERVAL = 0.6f;
    constexpr float NOTICE_DURATION_SKILL = 1.5f;
    constexpr int AI_REROLL_STEP_THRESHOLD = 2;
    constexpr int DOUBLE_ROLL_MULTIPLIER = 2;
    constexpr int AI_REVIVE_SEARCH_MAX_Y = 1;
}

// テクスチャ読み込みと初期手札の生成
void SkillManager::Initialize(ID3D11Device* device)
{
    CreateWICTextureFromFile(device, L"Resources/Textures/再振.png", nullptr,
                             m_cardTextures[SkillType::ReRoll].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/守護.png", nullptr,
                             m_cardTextures[SkillType::Shield].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/蘇生.png", nullptr,
                             m_cardTextures[SkillType::Revive].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/倍化.png", nullptr,
                             m_cardTextures[SkillType::DoubleRoll].ReleaseAndGetAddressOf());

    InitializeHandCards();
}

// カードデッキのシャッフルと手札へのランダム割り当て（全4種から手札作成）
void SkillManager::InitializeHandCards()
{
    std::vector<SkillCard> masterList = {{SkillType::ReRoll,
                                          L"リロール",
                                          L"出目が気に入らない時にサイコロをもう一度振り直せる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::ReRoll]},
                                         {SkillType::Shield,
                                          L"シールド",
                                          L"このターン中、自分の駒が相手に取られなくなる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::Shield]},
                                         {SkillType::Revive,
                                          L"復活",
                                          L"相手に倒された自分の駒を1体選んで陣地に復活させる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::Revive]},
                                         {SkillType::DoubleRoll,
                                          L"倍化",
                                          L"このターンの移動できるマス数が2倍になる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::DoubleRoll]}};

    static std::mt19937 rng(std::random_device{}());

    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < SkillManagerConstants::HAND_CARD_COUNT; ++i)
    {
        m_handCards[i] = masterList[i];
    }

    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < SkillManagerConstants::HAND_CARD_COUNT; ++i)
    {
        m_aiHandCards[i] = masterList[i];
    }

    m_isReviveSelecting = false;
    m_isDoubleRollActive = false;
    m_aiSkillTimer = 0.0f;
}

// プレイヤーのスキルカード発動処理
void SkillManager::ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                           bool hasMovedThisTurn, PlayerType myPlayerType)
{
    (void)hasMovedThisTurn;
    switch (card.type)
    {
        case SkillType::ReRoll:
            card.isUsed = true;
            m_isDoubleRollActive = false;
            scene->ResetMoveState();

            gameLogic.ResetDiceRollState();
            gameLogic.StartTurn();

            scene->AddLog(L"スキル「再振」を使用！ダイスを振り直します");
            break;

        case SkillType::DoubleRoll:
            if (gameLogic.GetRemainingSteps() > 0)
            {
                card.isUsed = true;
                m_isDoubleRollActive = true;
                gameLogic.SetRemainingSteps(gameLogic.GetRemainingSteps() *
                                            SkillStyleConstants::DOUBLE_ROLL_MULTIPLIER);
                scene->AddLog(L"スキル「倍化」を使用！移動数が2倍になりました");
            }
            break;

        case SkillType::Shield:
            card.isUsed = true;
            gameLogic.SetPlayerShield(myPlayerType, true);
            scene->AddLog(L"スキル「守護」を使用！このターン駒を守護します");
            break;

        case SkillType::Revive:
            m_isReviveSelecting = !m_isReviveSelecting;
            scene->AddLog(L"スキル「蘇生」の発動位置を選択中");
            break;
    }
}

// 通信相手からのスキル発動の適用処理
void SkillManager::ProcessRemoteSkillClick(SkillType skillType, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                           bool hasMovedThisTurn, PlayerType enemyPlayerType, int targetX, int targetY)
{
    (void)hasMovedThisTurn;
    switch (skillType)
    {
        case SkillType::ReRoll:
            m_isDoubleRollActive = false;
            scene->ResetMoveState();

            gameLogic.ResetDiceRollState();
            gameLogic.StartTurn();

            scene->TriggerTurnNotice(L"あいてがスキル「再振」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
            scene->AddLog(L"あいてがスキル「再振」を使用しました");
            break;

        case SkillType::DoubleRoll:
            if (gameLogic.GetRemainingSteps() > 0)
            {
                m_isDoubleRollActive = true;
                gameLogic.SetRemainingSteps(gameLogic.GetRemainingSteps() *
                                            SkillStyleConstants::DOUBLE_ROLL_MULTIPLIER);
                scene->TriggerTurnNotice(L"あいてがスキル「倍化」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
                scene->AddLog(L"あいてがスキル「倍化」を使用しました");
            }
            break;

        case SkillType::Shield:
            gameLogic.SetPlayerShield(enemyPlayerType, true);
            scene->TriggerTurnNotice(L"あいてがスキル「守護」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
            scene->AddLog(L"あいてがスキル「守護」を使用しました");
            break;

        case SkillType::Revive:
            if (targetX >= 0 && targetY >= 0)
            {
                gameLogic.RevivePieceAt(enemyPlayerType, targetX, targetY);
                scene->TriggerTurnNotice(L"あいてがスキル「蘇生」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
                scene->AddLog(L"あいてが駒を復帰させました");
            }
            break;
    }
}

// 蘇生スキルの実行処理
void SkillManager::ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                      PlayerType myPlayerType)
{
    if (gameLogic.RevivePieceAt(myPlayerType, targetX, targetY))
    {
        for (auto& card : m_handCards)
        {
            if (card.type == SkillType::Revive)
            {
                card.isUsed = true;
                break;
            }
        }
        m_isReviveSelecting = false;
        scene->AddLog(L"自分の駒を復帰させました");
    }
}

// CPU（2P）のスキル自動使用思考ロジック
void SkillManager::ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                       bool hasMovedThisTurn)
{
    if (gameLogic.IsAnyPieceMoving())
        return;

    m_aiSkillTimer += dt;
    if (m_aiSkillTimer < SkillStyleConstants::AI_SKILL_THINK_INTERVAL)
        return;

    for (auto& card : m_aiHandCards)
    {
        if (card.isUsed)
            continue;

        bool used = false;
        switch (card.type)
        {
            case SkillType::ReRoll:
            {
                if (gameLogic.GetRemainingSteps() <= SkillStyleConstants::AI_REROLL_STEP_THRESHOLD && !hasMovedThisTurn)
                {
                    gameLogic.ResetDiceRollState();
                    gameLogic.StartTurn();
                    used = true;
                    scene->TriggerTurnNotice(L"CPUがスキル「再振」を使用！",
                                             SkillStyleConstants::NOTICE_DURATION_SKILL);
                    scene->AddLog(L"CPUがスキル「再振」を発動しました");
                }
                break;
            }

            case SkillType::Shield:
            {
                gameLogic.SetPlayerShield(PlayerType::Player2, true);
                used = true;
                scene->TriggerTurnNotice(L"CPUがスキル「守護」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
                scene->AddLog(L"CPUがスキル「守護」を発動しました");
                break;
            }

            case SkillType::Revive:
            {
                if (gameLogic.HasDeadPiece(PlayerType::Player2))
                {
                    for (int y = 0; y <= SkillStyleConstants::AI_REVIVE_SEARCH_MAX_Y; ++y)
                    {
                        for (int x = 0; x < DiceBattleGame::BOARD_WIDTH; ++x)
                        {
                            if (gameLogic.RevivePieceAt(PlayerType::Player2, x, y))
                            {
                                used = true;
                                scene->TriggerTurnNotice(L"CPUがスキル「蘇生」を使用！",
                                                         SkillStyleConstants::NOTICE_DURATION_SKILL);
                                scene->AddLog(L"CPUがスキル「蘇生」を発動し、駒を復帰させました");
                                break;
                            }
                        }
                        if (used)
                            break;
                    }
                }
                break;
            }

            case SkillType::DoubleRoll:
            {
                if (gameLogic.GetRemainingSteps() > 0)
                {
                    m_isDoubleRollActive = true;
                    gameLogic.SetRemainingSteps(gameLogic.GetRemainingSteps() *
                                                SkillStyleConstants::DOUBLE_ROLL_MULTIPLIER);
                    used = true;
                    scene->TriggerTurnNotice(L"CPUがスキル「倍化」を使用！",
                                             SkillStyleConstants::NOTICE_DURATION_SKILL);
                    scene->AddLog(L"CPUがスキル「倍化」を発動しました");
                }
                break;
            }
        }

        if (used)
        {
            card.isUsed = true;
            m_aiSkillTimer = 0.0f;
            break;
        }
    }
}