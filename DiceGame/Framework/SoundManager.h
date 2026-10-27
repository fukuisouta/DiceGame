#pragma once

#include <Audio.h>
#include <memory>
#include <unordered_map>

// BGM識別用ID
enum class BGMType
{
    Title,
    Select,
    Play,
    Lobby
};

// SE（効果音）識別用ID
enum class SEType
{
    DiceRoll, // サイコロの転がる音
    // 必要に応じて ButtonClick や PieceMove など追加可能
};

class SoundManager
{
public:
    static SoundManager& GetInstance()
    {
        static SoundManager instance;
        return instance;
    }

    // 初期化およびサウンドファイルの読み込み
    bool Initialize();

    // フレームごとの更新（DirectXTK AudioEngineの更新用）
    void Update();

    // BGMの再生（ループ再生対応）
    void PlayBGM(BGMType type, bool loop = true, float volume = 0.5f);

    // BGMの停止
    void StopBGM();

    // SE（効果音）の再生（単発・ワンショット再生）
    void PlaySE(SEType type, float volume = 1.0f);

    // 音量調整（0.0f ～ 1.0f）
    void SetBGMVolume(float volume);
    void SetMasterVolume(float volume);

private:
    SoundManager() = default;
    ~SoundManager() = default;

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    std::unique_ptr<DirectX::AudioEngine> m_audioEngine;

    // BGM用リソース
    std::unordered_map<BGMType, std::unique_ptr<DirectX::SoundEffect>> m_bgmMap;
    std::unique_ptr<DirectX::SoundEffectInstance> m_currentBGMInstance;

    // SE用リソース
    std::unordered_map<SEType, std::unique_ptr<DirectX::SoundEffect>> m_seMap;

    BGMType m_currentBGM = BGMType::Title;
    bool m_isPlaying = false;
};