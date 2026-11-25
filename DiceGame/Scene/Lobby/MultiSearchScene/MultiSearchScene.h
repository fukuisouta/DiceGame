#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "Network/NetworkPackets.h"

class MultiSearchScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    MultiSearchScene() = default;
    ~MultiSearchScene() override = default;

    void OnEnter(GameContext& gameContext) override;
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    void Render(GameContext& gameContext) override;

private:
    std::vector<RoomSummaryInfo> m_roomList;
    float m_refreshTimer = 0.0f;
    std::string m_statusMsg = "";
    bool m_backToLobbyRequested = false;

    // パスワード入力ダイアログ用
    bool m_showPasswordPopup = false;
    uint32_t m_targetPrivateRoomId = 0;
    std::string m_targetHostName = "";
    char m_inputPassword[4] = "";
};