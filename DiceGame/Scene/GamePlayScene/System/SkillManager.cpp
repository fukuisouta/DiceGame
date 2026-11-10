#include "pch.h"
#include "SkillManager.h"
#include "../Base/GamePlaySceneBase.h"
#include <WICTextureLoader.h>
#include <algorithm>
#include <random>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// スキル処理パラメータ用定数定義
namespace SkillStyleConstants
{
    constexpr float AI_SKILL_THINK_INTERVAL = 0.6f; // AIがスキル思考を行う更新インターバル（秒）
    constexpr float NOTICE_DURATION_SKILL = 1.5f;   // スキル発動通知テロップの表示時間（秒）
    constexpr int AI_REROLL_STEP_THRESHOLD = 2;     // AIが振り直しを使用する残り歩数の閾値
    constexpr int DOUBLE_ROLL_MULTIPLIER = 2;       // 倍化スキル適用時の移動数倍率
    constexpr int AI_REVIVE_SEARCH_MAX_Y = 1;       // AIが蘇生コマを配置する自陣の最大Y座標
}

// スキルカードテクスチャの読み込みと初期化
void SkillManager::Initialize(ID3D11Device* device)
{
    CreateWICTextureFromFile(device, L"Resources/Textures/再振.png", nullptr,
                             m_cardTextures[SkillType::ReRoll].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/守護.png", nullptr,
                             m_cardTextures[SkillType::Shield].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/蘇生.png", nullptr,
                             m_cardTextures[SkillType::Revive].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/凍結.png", nullptr,
                             m_cardTextures[SkillType::Freeze].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/倍化.png", nullptr,
                             m_cardTextures[SkillType::DoubleRoll].ReleaseAndGetAddressOf());

    InitializeHandCards();
}

// カードデッキのシャッフルと手札配布
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
                                         {SkillType::Freeze,
                                          L"フリース",
                                          L"次の相手の順番を1回スキップして連続で行動できる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::Freeze]},
                                         {SkillType::DoubleRoll,
                                          L"倍化",
                                          L"このターンの移動できるマス数が2倍になる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                                          m_cardTextures[SkillType::DoubleRoll]}};

    static std::mt19937 rng(std::random_device{}());

    // プレイヤー（1P）へシャッフル配布
    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < SkillManagerConstants::HAND_CARD_COUNT; ++i)
    {
        m_handCards[i] = masterList[i];
    }

    // CPU（2P）へシャッフル配布
    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < SkillManagerConstants::HAND_CARD_COUNT; ++i)
    {
        m_aiHandCards[i] = masterList[i];
    }

    m_isFreezeNextTurn = false;
    m_isReviveSelecting = false;
    m_isDoubleRollActive = false;
    m_aiSkillTimer = 0.0f;
}

// プレイヤーのスキルカード発動処理
void SkillManager::ProcessPlayerSkillClick(SkillCard& card, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                           bool hasMovedThisTurn)
{
    (void)hasMovedThisTurn;
    switch (card.type)
    {
        case SkillType::ReRoll:
            card.isUsed = true;
            m_isDoubleRollActive = false;
            scene->ResetMoveState();
            gameLogic.SetRemainingSteps(0);
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

        case SkillType::Freeze:
            card.isUsed = true;
            m_isFreezeNextTurn = true;
            scene->AddLog(L"スキル「凍結」を使用！相手の次ターンをスキップします");
            break;

        case SkillType::Shield:
            card.isUsed = true;
            gameLogic.SetPlayerShield(PlayerType::Player1, true);
            scene->AddLog(L"スキル「守護」を使用！このターン駒を守護します");
            break;

        case SkillType::Revive:
            m_isReviveSelecting = !m_isReviveSelecting;
            scene->AddLog(L"スキル「蘇生」の発動位置を選択中");
            break;
    }
}

// 蘇生スキルの実行
void SkillManager::ExecuteReviveSkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic)
{
    if (gameLogic.RevivePieceAt(PlayerType::Player1, targetX, targetY))
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
                    gameLogic.SetRemainingSteps(0);
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

            case SkillType::Freeze:
            {
                m_isFreezeNextTurn = true;
                used = true;
                scene->TriggerTurnNotice(L"CPUがスキル「凍結」を使用！", SkillStyleConstants::NOTICE_DURATION_SKILL);
                scene->AddLog(L"CPUがスキル「凍結」を発動しました");
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