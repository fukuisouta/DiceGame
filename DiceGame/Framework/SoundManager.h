#pragma once

#include <Audio.h>
#include <memory>
#include <unordered_map>

// サウンド全般の定数定義
namespace SoundConstants
{
    constexpr float DEFAULT_BGM_VOLUME = 0.5f; // BGM再生時のデフォルト音量 (0.0f〜1.0f)
    constexpr float DEFAULT_SE_VOLUME = 1.0f;  // SE再生時のデフォルト音量 (0.0f〜1.0f)
    constexpr float SND_DEFAULT_PITCH = 0.0f;  // SE再生時のデフォルトピッチ変更値 (DEFAULT_PITCHから変更)
    constexpr float SND_DEFAULT_PAN = 0.0f;    // SE再生時のデフォルトパンニング (DEFAULT_PANから変更)
    constexpr int CWD_BUFFER_SIZE = 1024;      // カレントディレクトリ取得用文字列バッファサイズ
}

// 再生可能なBGMの識別ID列挙型
enum class BGMType
{
    Title,  // タイトル画面BGM
    Select, // モード選択画面BGM
    Play,   // メインゲームプレイ画面BGM
    Lobby   // ロビー画面BGM
};

// 再生可能なSE（効果音）の識別ID列挙型
enum class SEType
{
    DiceRoll, // サイコロを投擲・衝突させた際の効果音
};

// BGMおよびSEの読み込み・再生・停止・音量を一括管理するシングルトンクラス
class SoundManager
{
public:
    // シングルトンインスタンスを取得する関数
    static SoundManager& GetInstance()
    {
        static SoundManager instance;
        return instance;
    }

    // オーディオエンジンの初期化および各音源ファイルの読み込みを実行する関数
    // 戻り値: 初期化成功時 true, 失敗時 false
    bool Initialize();

    // 毎フレーム呼び出してオーディオエンジンの状態を更新する関数
    void Update();

    // 指定されたIDのBGMを再生する関数
    // 引数 type: 再生するBGMの種類, loop: ループ再生するかどうか, volume: 再生音量
    void PlayBGM(BGMType type, bool loop = true, float volume = SoundConstants::DEFAULT_BGM_VOLUME);

    // 現在再生中のBGMを停止する関数
    void StopBGM();

    // 指定されたIDのSE（効果音）をワンショット再生する関数
    // 引数 type: 再生するSEの種類, volume: 再生音量
    void PlaySE(SEType type, float volume = SoundConstants::DEFAULT_SE_VOLUME);

    // 現在再生中のBGM音量を変更する関数
    // 引数 volume: 設定する音量 (0.0f～1.0f)
    void SetBGMVolume(float volume);

    // 全体のマスター音量を変更する関数
    // 引数 volume: 設定する音量 (0.0f～1.0f)
    void SetMasterVolume(float volume);

private:
    SoundManager() = default;
    ~SoundManager() = default;

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    std::unique_ptr<DirectX::AudioEngine> m_audioEngine; // DirectXTK オーディオエンジン本体ポインタ

    std::unordered_map<BGMType, std::unique_ptr<DirectX::SoundEffect>> m_bgmMap; // BGMの音源リソースマップ
    std::unique_ptr<DirectX::SoundEffectInstance> m_currentBGMInstance;          // 現在再生中のBGMインスタンス

    std::unordered_map<SEType, std::unique_ptr<DirectX::SoundEffect>> m_seMap; // SEの音源リソースマップ

    BGMType m_currentBGM = BGMType::Title; // 現在設定されているBGMの種類
    bool m_isPlaying = false;              // BGMが再生中かどうかのフラグ
};