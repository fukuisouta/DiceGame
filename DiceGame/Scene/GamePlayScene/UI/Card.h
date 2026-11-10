#pragma once
#include <string>
#include <d3d11.h>
#include <SimpleMath.h>

// カード定数定義
namespace CardConstants
{
    constexpr float DEFAULT_CARD_WIDTH = 100.0f;  // デフォルトカード幅
    constexpr float DEFAULT_CARD_HEIGHT = 140.0f; // デフォルトカード高さ
}

// カードの種類（全7種類）
enum class CardType
{
    Destroy,    // 破壊: 相手の駒を1体指定して破棄
    Reroll,     // 再振: 自分のターンダイスを振り直す
    Protection, // 守護: 1ターンの間、自駒全体を保護
    Revive,     // 蘇生: 倒された自駒1体を指定位置（手前2行）に復帰
    Earthquake, // 地震: 盤面全体の駒の出目（回転）をランダムに変更
    Freeze,     // 凍結: 次の相手ターンをスキップ
    Double      // 倍化: 残り移動ステップ数を2倍にする
};

// スキルカードの単体データ・表示パラメータ・ヒットテスト管理クラス
class Card
{
public:
    Card(CardType type, const std::wstring& name, ID3D11ShaderResourceView* texture,
         const std::wstring& description = L"")
        : m_type(type), m_name(name), m_texture(texture), m_description(description), m_position(0.0f, 0.0f),
          m_size(CardConstants::DEFAULT_CARD_WIDTH, CardConstants::DEFAULT_CARD_HEIGHT), m_isHovered(false),
          m_isUsed(false)
    {
    }

    // カードの種別を取得する関数
    // 戻り値: CardType
    CardType GetType() const
    {
        return m_type;
    }

    // カード名を取得する関数
    // 戻り値: 名前文字列参照
    const std::wstring& GetName() const
    {
        return m_name;
    }

    // カードの説明テキストを取得する関数
    // 戻り値: 説明文字列参照
    const std::wstring& GetDescription() const
    {
        return m_description;
    }

    // カードテクスチャを取得する関数
    // 戻り値: シェーダーリソースビューポインタ
    ID3D11ShaderResourceView* GetTexture() const
    {
        return m_texture;
    }

    // カード表示位置を取得する関数
    // 戻り値: 2D座標ベクトル
    DirectX::SimpleMath::Vector2 GetPosition() const
    {
        return m_position;
    }

    // カード表示位置を設定する関数
    // 引数 pos: 2D座標
    void SetPosition(const DirectX::SimpleMath::Vector2& pos)
    {
        m_position = pos;
    }

    // カードサイズを取得する関数
    // 戻り値: サイズベクトル
    DirectX::SimpleMath::Vector2 GetSize() const
    {
        return m_size;
    }

    // カードサイズを設定する関数
    // 引数 size: サイズベクトル
    void SetSize(const DirectX::SimpleMath::Vector2& size)
    {
        m_size = size;
    }

    // ホバー状態を取得する関数
    // 戻り値: ホバー中であれば true
    bool IsHovered() const
    {
        return m_isHovered;
    }

    // ホバー状態を設定する関数
    // 引数 hovered: フラグ
    void SetHovered(bool hovered)
    {
        m_isHovered = hovered;
    }

    // 使用済み状態を取得する関数
    // 戻り値: 使用済みであれば true
    bool IsUsed() const
    {
        return m_isUsed;
    }

    // 使用済み状態を設定する関数
    // 引数 used: フラグ
    void SetUsed(bool used)
    {
        m_isUsed = used;
    }

    // カードの矩形エリア内判定（マウスヒットテスト用）を行う関数
    // 引数 point: 判定する2D座標
    // 戻り値: 範囲内であれば true
    bool Contains(const DirectX::SimpleMath::Vector2& point) const
    {
        return (point.x >= m_position.x && point.x <= m_position.x + m_size.x && point.y >= m_position.y &&
                point.y <= m_position.y + m_size.y);
    }

private:
    CardType m_type;                               // カード種別
    std::wstring m_name;                           // カード名
    std::wstring m_description;                    // カード説明文
    ID3D11ShaderResourceView* m_texture = nullptr; // テクスチャ参照

    DirectX::SimpleMath::Vector2 m_position; // 表示位置
    DirectX::SimpleMath::Vector2 m_size;     // 表示サイズ
    bool m_isHovered = false;                // ホバーフラグ
    bool m_isUsed = false;                   // 使用済みフラグ
};