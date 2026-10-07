#pragma once
#include <string>
#include <d3d11.h>
#include <SimpleMath.h>

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

class Card
{
public:
    Card(CardType type, const std::wstring& name, ID3D11ShaderResourceView* texture,
         const std::wstring& description = L"")
        : m_type(type), m_name(name), m_texture(texture), m_description(description), m_position(0.0f, 0.0f),
          m_size(100.0f, 140.0f), m_isHovered(false), m_isUsed(false)
    {
    }

    CardType GetType() const
    {
        return m_type;
    }
    const std::wstring& GetName() const
    {
        return m_name;
    }
    const std::wstring& GetDescription() const
    {
        return m_description;
    }
    ID3D11ShaderResourceView* GetTexture() const
    {
        return m_texture;
    }

    DirectX::SimpleMath::Vector2 GetPosition() const
    {
        return m_position;
    }
    void SetPosition(const DirectX::SimpleMath::Vector2& pos)
    {
        m_position = pos;
    }

    DirectX::SimpleMath::Vector2 GetSize() const
    {
        return m_size;
    }
    void SetSize(const DirectX::SimpleMath::Vector2& size)
    {
        m_size = size;
    }

    bool IsHovered() const
    {
        return m_isHovered;
    }
    void SetHovered(bool hovered)
    {
        m_isHovered = hovered;
    }

    bool IsUsed() const
    {
        return m_isUsed;
    }
    void SetUsed(bool used)
    {
        m_isUsed = used;
    }

    // カードの矩形エリア内判定（マウスヒットテスト用）
    bool Contains(const DirectX::SimpleMath::Vector2& point) const
    {
        return (point.x >= m_position.x && point.x <= m_position.x + m_size.x && point.y >= m_position.y &&
                point.y <= m_position.y + m_size.y);
    }

private:
    CardType m_type;
    std::wstring m_name;
    std::wstring m_description;
    ID3D11ShaderResourceView* m_texture = nullptr;

    DirectX::SimpleMath::Vector2 m_position;
    DirectX::SimpleMath::Vector2 m_size;
    bool m_isHovered = false;
    bool m_isUsed = false;
};