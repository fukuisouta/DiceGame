#pragma once

#include <vector>
#include <cstdint>
#include <cstring>

// バイト配列から安全に数値を読み出す汎用デシリアライザークラス
class PacketDataReader
{
public:
    // コンストラクタ
    // 引数 buffer: 解析対象のバイナリバッファ参照
    explicit PacketDataReader(const std::vector<uint8_t>& buffer)
        : m_buffer(buffer), m_offset(0)
    {}

    // 基本データ型を1つ読み進めて取得するテンプレート関数
    // 戻り値: 読み出した値
    template <typename T>
    T Read()
    {
        T value{};
        if (m_offset + sizeof(T) <= m_buffer.size())
        {
            std::memcpy(&value, &m_buffer[m_offset], sizeof(T));
            m_offset += sizeof(T);
        }
        return value;
    }

    // 指定されたサイズ分のデータが読み込み可能か判定する関数
    // 引数 size: チェックするバイト数
    // 戻り値: 読み込み可能な場合 true
    bool HasRemaining(size_t size = 1) const
    {
        return (m_offset + size <= m_buffer.size());
    }

private:
    const std::vector<uint8_t>& m_buffer; // バッファ参照
    size_t m_offset = 0;                  // 現在の読み込みオフセット位置
};