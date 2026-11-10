#include "pch.h"
#include "LobbyScene.h"
#include "Network/NetworkPackets.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// ロビーシーン表示スタイル定数定義
namespace LobbyStyleConstants
{
    constexpr float MATCH_ROOM_WIDTH_MAX = 680.0f;     // 対戦部屋ウィンドウの最大幅
    constexpr float MATCH_ROOM_WIDTH_MARGIN = 80.0f;   // 対戦部屋ウィンドウの左右マージン
    constexpr float MATCH_ROOM_HEIGHT = 230.0f;        // 対戦部屋ウィンドウの高さ
    constexpr float WINDOW_POS_Y_OFFSET = 50.0f;       // ウィンドウY位置の画面中央からのオフセット
    constexpr float START_BUTTON_HEIGHT = 52.0f;       // 対戦開始ボタンの高さ
    constexpr float CLIENT_WAIT_TEXT_Y_OFFSET = 10.0f; // クライアント待機テキストのYオフセット
    constexpr float FONT_SCALE_TITLE = 1.2f;           // タイトルおよび名前描画時のフォント倍率
    constexpr float FONT_SCALE_NORMAL = 1.0f;          // 通常描画時のフォント倍率
}

// シーン進入時の初期化処理
void LobbyScene::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Lobby, true);

    m_networkUI.Initialize();
    m_networkUI.SetVisible(true);

    m_opponentName = (const char*)u8"対戦相手を待っています...";
    m_hasSentPlayerInfo = false;
    m_startRequestedByHost = false;
}

// 毎フレームの更新処理
void LobbyScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    if (m_startRequestedByHost)
    {
        m_startRequestedByHost = false;
        sceneController.RequestSwitch(SceneId::SelectScene);
        return;
    }

    if (gameContext.networkManager.IsConnected())
    {
        if (!m_hasSentPlayerInfo)
        {
            MsgPlayerInfo infoPacket{};
            infoPacket.header.size = sizeof(MsgPlayerInfo);
            infoPacket.header.type = PacketType::CS_PlayerInfo;

            std::string myName = m_networkUI.GetPlayerName();
            strcpy_s(infoPacket.playerName, sizeof(infoPacket.playerName), myName.c_str());

            gameContext.networkManager.SendPacket(&infoPacket, sizeof(infoPacket));
            m_hasSentPlayerInfo = true;
        }

        std::vector<uint8_t> packet;
        while (gameContext.networkManager.PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

            if (header->type == PacketType::CS_PlayerInfo)
            {
                if (packet.size() >= sizeof(MsgPlayerInfo))
                {
                    MsgPlayerInfo* infoPacket = reinterpret_cast<MsgPlayerInfo*>(packet.data());
                    m_opponentName = infoPacket->playerName;
                }
            }
            else if (header->type == PacketType::CS_StartGame)
            {
                sceneController.RequestSwitch(SceneId::SelectScene);
                return;
            }
        }
    }
    else
    {
        m_hasSentPlayerInfo = false;
        m_opponentName = (const char*)u8"対戦相手を待っています...";
    }

    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Escape))
    {
        gameContext.networkManager.ForceDisconnect();
        sceneController.RequestSwitch(SceneId::TitleScene);
        return;
    }
}

// レンダリング処理
void LobbyScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    m_networkUI.Render(gameContext);

    if (gameContext.networkManager.IsConnected())
    {
        auto outputSize = gameContext.deviceResources.GetOutputSize();
        float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
        float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

        float matchWidth = (std::min)(LobbyStyleConstants::MATCH_ROOM_WIDTH_MAX,
                                      screenWidth - LobbyStyleConstants::MATCH_ROOM_WIDTH_MARGIN);
        float matchHeight = LobbyStyleConstants::MATCH_ROOM_HEIGHT;

        ImGui::SetNextWindowPos(
            ImVec2((screenWidth - matchWidth) * 0.5f, screenHeight * 0.5f + LobbyStyleConstants::WINDOW_POS_Y_OFFSET));
        ImGui::SetNextWindowSize(ImVec2(matchWidth, matchHeight));

        ImGui::Begin("Match Room Status", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_TITLE);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"対戦部屋の状況");
        ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Columns(2, "PlayersColumn", false);

        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"[ プレイヤー1（あなた：部屋主） ]");
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_TITLE);
            ImGui::Text("%s", m_networkUI.GetPlayerName().c_str());
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);

            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.00f), (const char*)u8"[ プレイヤー2（対戦相手） ]");
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_TITLE);
            ImGui::Text("%s", m_opponentName.c_str());
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.00f), (const char*)u8"[ プレイヤー1（部屋主） ]");
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_TITLE);
            ImGui::Text("%s", m_opponentName.c_str());
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);

            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"[ プレイヤー2（あなた） ]");
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_TITLE);
            ImGui::Text("%s", m_networkUI.GetPlayerName().c_str());
            ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            if (ImGui::Button((const char*)u8"対戦を開始する", ImVec2(-1, LobbyStyleConstants::START_BUTTON_HEIGHT)))
            {
                PacketHeader msg{};
                msg.size = sizeof(PacketHeader);
                msg.type = PacketType::CS_StartGame;
                gameContext.networkManager.SendPacket(&msg, sizeof(msg));

                m_startRequestedByHost = true;
            }
        }
        else
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LobbyStyleConstants::CLIENT_WAIT_TEXT_Y_OFFSET);
            ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.30f, 1.00f),
                               (const char*)u8"準備完了！ 部屋主がゲームを開始するのを待っています...");
        }

        ImGui::End();
    }

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    debugRenderer.Render(context, &gameContext.commonStates);
}