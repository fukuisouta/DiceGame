#pragma once
#include "Card.h"
#include <vector>
#include <memory>
#include <random>
#include <algorithm>

// デッキマネージャー定数定義
namespace DeckConstants
{
    constexpr size_t TOTAL_CARD_TYPES = 7;   // 全カード種類の総数
    constexpr size_t HAND_DRAW_COUNT = 3;    // 手札ドロー枚数
    constexpr float HAND_START_X = 300.0f;   // 手札配置開始X座標
    constexpr float HAND_SPACING_X = 150.0f; // 手札配置間隔
    constexpr float HAND_POS_Y = 600.0f;     // 手札配置Y座標
}

// カードデッキ全体の生成、初期化、シャッフル手札ドローの管理クラス
class CardDeckManager
{
private:
    std::vector<std::shared_ptr<Card>> m_masterDeck; // 全カードリスト
    std::mt19937 m_randomEngine;                     // 乱数生成器

public:
    CardDeckManager()
    {
        std::random_device rd;
        m_randomEngine = std::mt19937(rd());
    }

    // 全7種類のカードを初期化・マスターデッキへ登録する関数
    // 引数 textures: テクスチャ配列参照
    void InitializeDeck(const std::vector<ID3D11ShaderResourceView*>& textures)
    {
        m_masterDeck.clear();
        if (textures.size() < DeckConstants::TOTAL_CARD_TYPES)
            return;

        m_masterDeck.push_back(
            std::make_shared<Card>(CardType::Destroy, L"Destroy", textures[0], L"あいてのこまをはかいする"));
        m_masterDeck.push_back(std::make_shared<Card>(CardType::Reroll, L"Reroll", textures[1], L"ダイスをふりなおす"));
        m_masterDeck.push_back(std::make_shared<Card>(CardType::Protection, L"Protection", textures[2],
                                                      L"1ターンのあいだ、じぶんのコマをまもる"));
        m_masterDeck.push_back(std::make_shared<Card>(CardType::Revive, L"Revive", textures[3],
                                                      L"たおされたみかたのこまをてまえ２ぎょうにふっかつさせる"));
        m_masterDeck.push_back(std::make_shared<Card>(CardType::Earthquake, L"Earthquake", textures[4],
                                                      L"ばんめんのぜんこまのでめをランダムにへんこうする"));
        m_masterDeck.push_back(
            std::make_shared<Card>(CardType::Freeze, L"Freeze", textures[5], L"つぎのあいてのターンをスキップさせる"));
        m_masterDeck.push_back(
            std::make_shared<Card>(CardType::Double, L"Double", textures[6], L"のこりいどうステップすうを2ばいにする"));
    }

    // 重複なしでカードを抽出し手札領域に配置ドローする関数
    // 戻り値: 手札カードリスト
    std::vector<std::shared_ptr<Card>> DrawHand()
    {
        if (m_masterDeck.empty())
            return {};

        auto tempDeck = m_masterDeck;
        std::shuffle(tempDeck.begin(), tempDeck.end(), m_randomEngine);

        size_t drawCount = (std::min)(DeckConstants::HAND_DRAW_COUNT, tempDeck.size());
        std::vector<std::shared_ptr<Card>> hand(tempDeck.begin(), tempDeck.begin() + drawCount);

        for (size_t i = 0; i < hand.size(); ++i)
        {
            float x = DeckConstants::HAND_START_X + static_cast<float>(i) * DeckConstants::HAND_SPACING_X;
            float y = DeckConstants::HAND_POS_Y;
            hand[i]->SetPosition(DirectX::SimpleMath::Vector2(x, y));
            hand[i]->SetUsed(false);
        }

        return hand;
    }
};