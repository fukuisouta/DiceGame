#pragma once

#include <memory>
#include <vector>
#include <string>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "Framework/SoundManager.h"

class LobbyScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    LobbyScene() = default;
    ~LobbyScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    enum class LobbyStep
    {
        NameAndConnect,  // 1. 名前登録 ＆ サーバー接続
        ChooseAction,    // 2. ルーム作成 or ルーム検索 選択
        CreateRoomSetup, // 3. ルーム作成設定（モード選択 ＆ フリー/3桁PW選択）
        WaitingForGuest  // 4. 対戦相手の入室待機中
    };

    LobbyStep m_step = LobbyStep::NameAndConnect;

    char m_playerNameBuffer[32] = "Player";
    std::string m_statusMsg = "";

    // ルーム作成用パラメータ
    int m_selectedGameMode = 0; // 0: オリジナルモード, 1: アドバンスモード
    int m_selectedPrivacy = 0;  // 0: フリー, 1: プライベート(3桁PW)
    char m_passwordBuffer[4] = "123";
    uint32_t m_createdRoomId = 0;
};