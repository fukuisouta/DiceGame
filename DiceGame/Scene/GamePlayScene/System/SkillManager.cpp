#include "pch.h"
#include "SkillManager.h"
#include "../Base/GamePlaySceneBase.h"
#include <WICTextureLoader.h>
#include <algorithm>
#include <random>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// スキルカードテクスチャの読み込みと初期化
void SkillManager::Initialize(ID3D11Device* device)
{
    /*CreateWICTextureFromFile(device, L"Resources/Textures/破壊.png", nullptr,
                             m_cardTextures[SkillType::Destroy].ReleaseAndGetAddressOf());*/
    CreateWICTextureFromFile(device, L"Resources/Textures/再振.png", nullptr,
                             m_cardTextures[SkillType::ReRoll].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/守護.png", nullptr,
                             m_cardTextures[SkillType::Shield].ReleaseAndGetAddressOf());
    CreateWICTextureFromFile(device, L"Resources/Textures/蘇生.png", nullptr,
                             m_cardTextures[SkillType::Revive].ReleaseAndGetAddressOf());
    /* CreateWICTextureFromFile(device, L"Resources/Textures/地震.png", nullptr,
                              m_cardTextures[SkillType::Earthquake].ReleaseAndGetAddressOf());*/
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
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::ReRoll]},
                                         {SkillType::Shield,
                                          L"シールド",
                                          L"このターン中、自分の駒が相手に取られなくなる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::Shield]},
                                         {SkillType::Revive,
                                          L"復活",
                                          L"相手に倒された自分の駒を1体選んで陣地に復活させる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::Revive]},
                                         {SkillType::Freeze,
                                          L"フリース",
                                          L"次の相手の順番を1回スキップして連続で行動できる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::Freeze]},
                                         {SkillType::DoubleRoll,
                                          L"倍化",
                                          L"このターンの移動できるマス数が2倍になる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::DoubleRoll]},
                                       /*  {SkillType::Destroy,
                                          L"Destroy",
                                          L"相手の駒を1つ選んで破壊する",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::Destroy]},*/
                                        /* {SkillType::Earthquake,
                                          L"Earthquake",
                                          L"盤上のすべての駒の出目をランダムに変化させる",
                                          {0, 0, 0, 0},
                                          false,
                                          false,
                                          Vector4(1, 1, 1, 1),
                                          m_cardTextures[SkillType::Earthquake]}*/};

    static std::mt19937 rng(std::random_device{}());

    // プレイヤー（1P）へ3枚シャッフル配布
    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < 3; ++i)
        m_handCards[i] = masterList[i];

    // CPU（2P）へ3枚シャッフル配布
    std::shuffle(masterList.begin(), masterList.end(), rng);
    for (size_t i = 0; i < 3; ++i)
        m_aiHandCards[i] = masterList[i];

    m_isFreezeNextTurn = false;
    m_isReviveSelecting = false;
    // m_isDestroySelecting = false;
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
            /* case SkillType::Destroy:
                 m_isDestroySelecting = !m_isDestroySelecting;
                 m_isReviveSelecting = false;
                 scene->AddLog(L"スキル「破壊」の発動対象を選択中");
                 break;*/
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
                gameLogic.SetRemainingSteps(gameLogic.GetRemainingSteps() * 2);
                scene->AddLog(L"スキル「倍化」を使用！移動数が2倍になりました");
            }
            break;
            /*   case SkillType::Earthquake:
                   card.isUsed = true;
                   ExecuteEarthquakeSkill(scene, gameLogic);
                   scene->AddLog(L"スキル「地震」を使用！全駒の出目を変更しました");
                   break;*/
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
            // m_isDestroySelecting = false;
            scene->AddLog(L"スキル「蘇生」の発動位置を選択中");
            break;
    }
}

// void SkillManager::ExecuteDestroySkill(int targetX, int targetY, GamePlaySceneBase* scene, DiceBattleGame& gameLogic)
//{
//     const auto* targetPiece = gameLogic.GetPieceAt(targetX, targetY);
//     if (targetPiece && targetPiece->owner == PlayerType::Player2)
//     {
//         gameLogic.RemovePieceAt(targetX, targetY);
//         for (auto& card : m_handCards)
//         {
//             if (card.type == SkillType::Destroy)
//             {
//                 card.isUsed = true;
//                 break;
//             }
//         }
//         m_isDestroySelecting = false;
//         scene->TriggerTurnNotice(L"相手の駒を破壊した！", 1.5f);
//     }
// }

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

// void SkillManager::ExecuteEarthquakeSkill(GamePlaySceneBase* scene, DiceBattleGame& gameLogic)
//{
//     static std::mt19937 rng(std::random_device{}());
//     std::uniform_real_distribution<float> dist(-XM_PI, XM_PI);
//
//     auto& pieces = const_cast<std::vector<BoardPiece>&>(gameLogic.GetPieces());
//     for (auto& piece : pieces)
//     {
//         if (piece.isActive && piece.diceInstance && piece.type != PieceType::King)
//         {
//             Quaternion randomRot = Quaternion::CreateFromYawPitchRoll(dist(rng), dist(rng), dist(rng));
//             piece.startRot = piece.diceInstance->GetRotation();
//             piece.targetRot = randomRot;
//             piece.rotProgress = 0.0f;
//         }
//     }
//     scene->TriggerTurnNotice(L"地震発生！出目が変化した！", 1.8f);
// }

// CPU（2P）のスキル自動使用思考ロジック
void SkillManager::ProcessAISkillLogic(float dt, GamePlaySceneBase* scene, DiceBattleGame& gameLogic,
                                       bool hasMovedThisTurn)
{
    if (gameLogic.IsAnyPieceMoving())
        return;

    m_aiSkillTimer += dt;
    if (m_aiSkillTimer < 0.6f)
        return;

    for (auto& card : m_aiHandCards)
    {
        if (card.isUsed)
            continue;

        bool used = false;
        switch (card.type)
        { /*
             case SkillType::Destroy:
             {
                 for (const auto& piece : gameLogic.GetPieces())
                 {
                     if (piece.isActive && piece.owner == PlayerType::Player1)
                     {
                         gameLogic.RemovePieceAt(piece.gridX, piece.gridY);
                         used = true;
                         scene->TriggerTurnNotice(L"CPUがスキル「破壊」を使用！", 1.5f);
                         scene->AddLog(L"CPUがスキル「破壊」を発動し、あなたの駒を破壊しました");
                         break;
                     }
                 }
                 break;
             }*/
            case SkillType::ReRoll:
            {
                // 歩数が少ない場合に振り直しスキルを使用
                if (gameLogic.GetRemainingSteps() <= 2 && !hasMovedThisTurn)
                {
                    gameLogic.SetRemainingSteps(0);
                    gameLogic.StartTurn();
                    used = true;
                    scene->TriggerTurnNotice(L"CPUがスキル「再振」を使用！", 1.5f);
                    scene->AddLog(L"CPUがスキル「再振」を発動しました");
                }
                break;
            }
            case SkillType::Shield:
            {
                gameLogic.SetPlayerShield(PlayerType::Player2, true);
                used = true;
                scene->TriggerTurnNotice(L"CPUがスキル「守護」を使用！", 1.5f);
                scene->AddLog(L"CPUがスキル「守護」を発動しました");
                break;
            }
            case SkillType::Revive:
            {
                // 撃破されたコマが存在すれば陣地の空きマスに復活
                if (gameLogic.HasDeadPiece(PlayerType::Player2))
                {
                    for (int y = 0; y <= 1; ++y)
                    {
                        for (int x = 0; x < DiceBattleGame::BOARD_WIDTH; ++x)
                        {
                            if (gameLogic.RevivePieceAt(PlayerType::Player2, x, y))
                            {
                                used = true;
                                scene->TriggerTurnNotice(L"CPUがスキル「蘇生」を使用！", 1.5f);
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
                scene->TriggerTurnNotice(L"CPUがスキル「凍結」を使用！", 1.5f);
                scene->AddLog(L"CPUがスキル「凍結」を発動しました");
                break;
            }
            case SkillType::DoubleRoll:
            {
                if (gameLogic.GetRemainingSteps() > 0)
                {
                    m_isDoubleRollActive = true;
                    gameLogic.SetRemainingSteps(gameLogic.GetRemainingSteps() * 2);
                    used = true;
                    scene->TriggerTurnNotice(L"CPUがスキル「倍化」を使用！", 1.5f);
                    scene->AddLog(L"CPUがスキル「倍化」を発動しました");
                }
                break;
            }
                /* case SkillType::Earthquake:
                 {
                     ExecuteEarthquakeSkill(scene, gameLogic);
                     used = true;
                     scene->TriggerTurnNotice(L"CPUがスキル「地震」を使用！", 1.5f);
                     scene->AddLog(L"CPUがスキル「地震」を発動しました");
                     break;
                 }*/
        }

        if (used)
        {
            card.isUsed = true;
            m_aiSkillTimer = 0.0f;
            break;
        }
    }
}