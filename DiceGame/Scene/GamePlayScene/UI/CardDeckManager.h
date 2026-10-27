#pragma once
#include "Card.h"
#include <vector>
#include <memory>
#include <random>
#include <algorithm>

// ============================================================================
// カードデッキ全体の生成、初期化、シャッフル手札ドローの管理クラス
// ============================================================================
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

    // 全7種類のカードを初期化・マスターデッキへ登録[cite: 53]
    void InitializeDeck(const std::vector<ID3D11ShaderResourceView*>& textures)
    {
        m_masterDeck.clear();
        if (textures.size() < 7)
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

    // 7枚の中から重複なしで3枚を抽出し手札領域に配置[cite: 53]
    std::vector<std::shared_ptr<Card>> DrawHand()
    {
        if (m_masterDeck.empty())
            return {};

        auto tempDeck = m_masterDeck;
        std::shuffle(tempDeck.begin(), tempDeck.end(), m_randomEngine);

        size_t drawCount = (std::min)(size_t(3), tempDeck.size());
        std::vector<std::shared_ptr<Card>> hand(tempDeck.begin(), tempDeck.begin() + drawCount);

        // 画面下に150px間隔で配置[cite: 53]
        for (size_t i = 0; i < hand.size(); ++i)
        {
            float x = 300.0f + static_cast<float>(i) * 150.0f;
            float y = 600.0f;
            hand[i]->SetPosition(DirectX::SimpleMath::Vector2(x, y));
            hand[i]->SetUsed(false);
        }

        return hand;
    }
};