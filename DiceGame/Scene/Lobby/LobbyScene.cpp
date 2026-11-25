#include "pch.h"
#pragma execution_character_set("utf-8")

#include "LobbyScene.h"
#include "Network/NetworkPackets.h"
#include "Network/NetworkManager.h"
#include "imgui.h"
#include <cstring>
#include <cctype>
#include <algorithm>

using namespace DirectX;

namespace
{
    constexpr const char* SERVER_IP = "129.225.179.35";
    constexpr uint16_t SERVER_PORT = 46860;

    bool IsValid3DigitPassword(const char* pw)
    {
        if (std::strlen(pw) != 3)
            return false;
        return std::isdigit(static_cast<unsigned char>(pw[0])) && std::isdigit(static_cast<unsigned char>(pw[1])) &&
               std::isdigit(static_cast<unsigned char>(pw[2]));
    }
}

void LobbyScene::OnEnter(GameContext& gameContext)
{
    SoundManager::GetInstance().PlayBGM(BGMType::Lobby, true);

    // すでにサーバー接続済み（マルチサーチから戻ってきた場合など）なら行動選択画面へ
    if (gameContext.networkManager.IsConnected())
    {
        m_step = LobbyStep::ChooseAction;
        strncpy_s(m_playerNameBuffer, sizeof(m_playerNameBuffer),
                  gameContext.networkManager.GetLocalPlayerName().c_str(), _TRUNCATE);
    }
    else
    {
        m_step = LobbyStep::NameAndConnect;
    }

    m_createdRoomId = 0;
    m_statusMsg.clear();
}

void LobbyScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    // 「ルームを検索する」ボタンが押されていたらマルチサーチシーンへ遷移
    if (m_statusMsg == "GOTO_SEARCH")
    {
        m_statusMsg.clear();
        sceneController.RequestSwitch(SceneId::MultiSearchScene);
        return;
    }
    // 「タイトルへ戻る」ボタンが押されていたらタイトルシーンへ遷移
    if (m_statusMsg == "GOTO_TITLE")
    {
        m_statusMsg.clear();
        gameContext.networkManager.ForceDisconnect();
        sceneController.RequestSwitch(SceneId::TitleScene);
        return;
    }

    gameContext.networkManager.Update();

    if (gameContext.networkManager.IsConnected())
    {
        std::vector<uint8_t> packet;
        while (gameContext.networkManager.PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

            // ルーム作成完了通知
            if (header->type == PacketType::SC_RoomCreated && packet.size() >= sizeof(MsgRoomCreated))
            {
                auto* createdPkt = reinterpret_cast<MsgRoomCreated*>(packet.data());
                m_createdRoomId = createdPkt->roomId;
                m_step = LobbyStep::WaitingForGuest;
            }
            // マッチング成立通知（相手が入室した瞬間に直接ゲームシーンへ移行！）
            else if (header->type == PacketType::SC_MatchStart && packet.size() >= sizeof(MsgMatchStart))
            {
                auto* matchPkt = reinterpret_cast<MsgMatchStart*>(packet.data());
                gameContext.networkManager.SetAssignedPlayerType(matchPkt->assignedPlayerType);
                gameContext.networkManager.SetOpponentName(matchPkt->opponentName);

                if (matchPkt->gameMode == 1)
                {
                    sceneController.RequestSwitch(SceneId::AdvanceGamePlayScene);
                }
                else
                {
                    sceneController.RequestSwitch(SceneId::OriginalGamePlayScene);
                }
                return;
            }
        }
    }
    else if (m_step != LobbyStep::NameAndConnect)
    {
        m_step = LobbyStep::NameAndConnect;
        m_statusMsg = (const char*)u8"サーバーから切断されました。";
    }

    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Escape))
    {
        if (m_step == LobbyStep::WaitingForGuest)
        {
            gameContext.networkManager.SendCancelRoom();
            m_step = LobbyStep::ChooseAction;
        }
        else
        {
            gameContext.networkManager.ForceDisconnect();
            sceneController.RequestSwitch(SceneId::TitleScene);
        }
    }
}

void LobbyScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 6.0f;
    style.WindowPadding = ImVec2(24.0f, 24.0f);
    style.ItemSpacing = ImVec2(12.0f, 12.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.08f, 0.14f, 0.90f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.13f, 0.22f, 0.85f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.42f, 0.72f, 0.95f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.62f, 0.95f, 1.00f);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));
    ImGui::Begin("FullLobbyScreen", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float panelW = (std::min)(680.0f, screenWidth - 60.0f);
    float panelH = (std::min)(520.0f, screenHeight - 60.0f);
    ImGui::SetCursorPos(ImVec2((screenWidth - panelW) * 0.5f, (screenHeight - panelH) * 0.5f));

    ImGui::BeginChild("LobbyCard", ImVec2(panelW, panelH), true);

    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"オンライン対戦ロビー");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    // --- ステップ1: 名前登録＆サーバー接続 ---
    if (m_step == LobbyStep::NameAndConnect)
    {
        ImGui::Text((const char*)u8"プレイヤー名を登録してください");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##NameInput", (const char*)u8"プレイヤー名を入力...", m_playerNameBuffer,
                                 sizeof(m_playerNameBuffer));

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"名前を確定してサーバーに接続する", ImVec2(-1.0f, 48.0f)))
        {
            if (std::strlen(m_playerNameBuffer) == 0)
            {
                m_statusMsg = (const char*)u8"プレイヤー名を入力してください。";
            }
            else if (gameContext.networkManager.ConnectToHost(SERVER_IP, SERVER_PORT))
            {
                gameContext.networkManager.SetLocalPlayerName(m_playerNameBuffer);
                m_statusMsg.clear();
                m_step = LobbyStep::ChooseAction;
            }
            else
            {
                m_statusMsg =
                    (const char*)u8"サーバー接続に失敗しました。通信環境またはサーバー稼働状態を確認してください。";
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"タイトルへ戻る", ImVec2(-1.0f, 40.0f)))
        {
            m_statusMsg = "GOTO_TITLE";
        }

        if (!m_statusMsg.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", m_statusMsg.c_str());
        }
    }
    // --- ステップ2: ルーム作成 or ルーム検索 選択 ---
    else if (m_step == LobbyStep::ChooseAction)
    {
        ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), (const char*)u8"接続完了！ プレイヤー名: %s",
                           gameContext.networkManager.GetLocalPlayerName().c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text((const char*)u8"プレイスタイルを選択してください：");
        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"ルームを作成する（ホストとして待機）", ImVec2(-1.0f, 56.0f)))
        {
            m_statusMsg.clear();
            m_step = LobbyStep::CreateRoomSetup;
        }

        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"ルームを検索する（マルチサーチへ移動）", ImVec2(-1.0f, 56.0f)))
        {
            m_statusMsg = "GOTO_SEARCH";
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"サーバーから切断して戻る", ImVec2(-1.0f, 38.0f)))
        {
            gameContext.networkManager.ForceDisconnect();
            m_step = LobbyStep::NameAndConnect;
        }
    }
    // --- ステップ3: ルーム作成設定（モード選択 ＆ フリー/3桁パスワード） ---
    else if (m_step == LobbyStep::CreateRoomSetup)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), (const char*)u8"【ルーム作成設定】");
        ImGui::Spacing();

        ImGui::Text((const char*)u8"◆ 遊ぶゲームモードを選択:");
        ImGui::RadioButton((const char*)u8"オリジナルモード（クラシックルール）", &m_selectedGameMode, 0);
        ImGui::RadioButton((const char*)u8"アドバンスモード（スキルカードあり）", &m_selectedGameMode, 1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text((const char*)u8"◆ 公開設定を選択:");
        ImGui::RadioButton((const char*)u8"フリー（誰でも検索から参加可能）", &m_selectedPrivacy, 0);
        ImGui::RadioButton((const char*)u8"プライベート（数字3桁のパスワードを設定）", &m_selectedPrivacy, 1);

        if (m_selectedPrivacy == 1)
        {
            ImGui::Indent(24.0f);
            ImGui::Text((const char*)u8"パスワード（半角数字3桁）:");
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputText("##RoomPw", m_passwordBuffer, sizeof(m_passwordBuffer), ImGuiInputTextFlags_CharsDecimal);
            ImGui::Unindent(24.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"この設定でルームを作成する", ImVec2(-1.0f, 48.0f)))
        {
            if (m_selectedPrivacy == 1 && !IsValid3DigitPassword(m_passwordBuffer))
            {
                m_statusMsg = (const char*)u8"パスワードは半角数字3桁（例: 123）で入力してください。";
            }
            else
            {
                m_statusMsg.clear();
                gameContext.networkManager.SendCreateRoom(static_cast<uint8_t>(m_selectedGameMode),
                                                          (m_selectedPrivacy == 1),
                                                          (m_selectedPrivacy == 1) ? m_passwordBuffer : "");
            }
        }

        ImGui::Spacing();
        if (ImGui::Button((const char*)u8"戻る", ImVec2(140.0f, 36.0f)))
        {
            m_statusMsg.clear();
            m_step = LobbyStep::ChooseAction;
        }

        if (!m_statusMsg.empty() && m_statusMsg != "GOTO_SEARCH")
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", m_statusMsg.c_str());
        }
    }
    // --- ステップ4: 対戦相手待機画面 ---
    else if (m_step == LobbyStep::WaitingForGuest)
    {
        ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), (const char*)u8"ルームを作成しました！（Room #%u）",
                           m_createdRoomId);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text((const char*)u8"ホスト名: %s", gameContext.networkManager.GetLocalPlayerName().c_str());
        ImGui::Text((const char*)u8"ゲームモード: %s",
                    (m_selectedGameMode == 1) ? (const char*)u8"アドバンスモード" : (const char*)u8"オリジナルモード");

        if (m_selectedPrivacy == 1)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                               (const char*)u8"公開区分: プライベート [ パスワード: %s ]", m_passwordBuffer);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), (const char*)u8"公開区分: フリー（誰でも参加可）");
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f),
                           (const char*)u8"対戦相手の入室を待っています... 入室すると自動でゲームが始まります！");

        ImGui::Spacing();
        ImGui::Spacing();
        if (ImGui::Button((const char*)u8"ルームを解散して戻る", ImVec2(-1.0f, 42.0f)))
        {
            gameContext.networkManager.SendCancelRoom();
            m_step = LobbyStep::ChooseAction;
        }
    }

    ImGui::EndChild();
    ImGui::End();

    gameContext.debugRenderer.Render(context, &gameContext.commonStates);
}