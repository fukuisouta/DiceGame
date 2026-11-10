#include "pch.h"
#include "SoundManager.h"
#include <windows.h>
#include <direct.h>

// オーディオエンジンの初期化と音声ファイルのロード
bool SoundManager::Initialize()
{
    // デバッグ確認用に現在の作業ディレクトリを出力
    char cwd[SoundConstants::CWD_BUFFER_SIZE];
    if (_getcwd(cwd, sizeof(cwd)) != nullptr)
    {
        OutputDebugStringA("[SoundManager] Current Directory: ");
        OutputDebugStringA(cwd);
        OutputDebugStringA("\n");
    }

    // デバッグビルド時はデバッグ用オーディオエンジンフラグを設定
    DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_Default;
#if defined(_DEBUG)
    eflags |= DirectX::AudioEngine_Debug;
#endif

    try
    {
        m_audioEngine = std::make_unique<DirectX::AudioEngine>(eflags);
        OutputDebugStringA("[SoundManager] AudioEngine 生成成功\n");
    }
    catch (...)
    {
        OutputDebugStringA("[SoundManager] AudioEngine の生成に失敗しました。(COM未初期化等の可能性)\n");
        return false;
    }

    // 音声ファイルを個別に読み込んでマップへ割り当てるヘルパーラムダ関数
    auto LoadSound = [this](const wchar_t* path, auto& map, auto type, const char* debugName)
    {
        try
        {
            map[type] = std::make_unique<DirectX::SoundEffect>(m_audioEngine.get(), path);
            OutputDebugStringA("[SoundManager] 読み込み成功: ");
            OutputDebugStringA(debugName);
            OutputDebugStringA("\n");
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA("[SoundManager] 読み込み例外 (");
            OutputDebugStringA(debugName);
            OutputDebugStringA("): ");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
        }
        catch (...)
        {
            OutputDebugStringA("[SoundManager] 読み込み失敗 (");
            OutputDebugStringA(debugName);
            OutputDebugStringA("): フォーマット不正またはファイル不在\n");
        }
    };

    // --- BGMリソースの読み込み ---
    LoadSound(L"Resources/Sounds/TitleBGM.wav", m_bgmMap, BGMType::Title, "TitleBGM");
    LoadSound(L"Resources/Sounds/SelectBGM.wav", m_bgmMap, BGMType::Select, "SelectBGM");
    LoadSound(L"Resources/Sounds/PlayBGM.wav", m_bgmMap, BGMType::Play, "PlayBGM");
    LoadSound(L"Resources/Sounds/LobbyBGM.wav", m_bgmMap, BGMType::Lobby, "LobbyBGM");

    // --- SEリソースの読み込み ---
    LoadSound(L"Resources/Sounds/DiceRoll.wav", m_seMap, SEType::DiceRoll, "DiceRoll");

    return true;
}

// 毎フレームのオーディオエンジン更新処理
void SoundManager::Update()
{
    if (m_audioEngine && !m_audioEngine->Update())
    {
        // クリティカルエラー発生時のログ出力
        if (m_audioEngine->IsCriticalError())
        {
            OutputDebugStringA("[SoundManager] AudioEngine Critical Error!\n");
        }
    }
}

// 指定した種類のBGMを再生
void SoundManager::PlayBGM(BGMType type, bool loop, float volume)
{
    // 同一BGMが既に再生中の場合は処理をスキップ
    if (m_isPlaying && m_currentBGM == type && m_currentBGMInstance)
    {
        if (m_currentBGMInstance->GetState() == DirectX::SoundState::PLAYING)
            return;
    }

    StopBGM();

    // 音源マップから検索して再生インスタンスを生成
    auto it = m_bgmMap.find(type);
    if (it != m_bgmMap.end() && it->second)
    {
        m_currentBGMInstance = it->second->CreateInstance();
        if (m_currentBGMInstance)
        {
            m_currentBGMInstance->SetVolume(volume);
            m_currentBGMInstance->Play(loop);
            m_currentBGM = type;
            m_isPlaying = true;
            OutputDebugStringA("[SoundManager] BGM再生開始\n");
        }
    }
    else
    {
        OutputDebugStringA("[SoundManager] 指定されたBGMデータが見つかりません。\n");
    }
}

// 現在再生中のBGMを停止
void SoundManager::StopBGM()
{
    if (m_currentBGMInstance)
    {
        m_currentBGMInstance->Stop(true);
        m_currentBGMInstance.reset();
    }
    m_isPlaying = false;
}

// 指定した種類のSEをワンショット再生
void SoundManager::PlaySE(SEType type, float volume)
{
    auto it = m_seMap.find(type);
    if (it != m_seMap.end() && it->second)
    {
        // マクロ衝突回避用に改名された定数 (SND_DEFAULT_PITCH / SND_DEFAULT_PAN) を使用
        it->second->Play(volume, SoundConstants::SND_DEFAULT_PITCH, SoundConstants::SND_DEFAULT_PAN);
    }
    else
    {
        OutputDebugStringA("[SoundManager] 指定されたSEデータが見つかりません。\n");
    }
}

// 再生中のBGM音量を変更
void SoundManager::SetBGMVolume(float volume)
{
    if (m_currentBGMInstance)
    {
        m_currentBGMInstance->SetVolume(volume);
    }
}

// オーディオエンジン全体のマスター音量を変更
void SoundManager::SetMasterVolume(float volume)
{
    if (m_audioEngine)
    {
        m_audioEngine->SetMasterVolume(volume);
    }
}