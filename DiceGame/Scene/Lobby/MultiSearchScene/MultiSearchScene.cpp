#include "pch.h"
#include "MultiSearchScene.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

void MultiSearchScene::OnEnter(GameContext& gameContext)
{
    m_roomList.clear();
    m_refreshTimer = 0.0f;
    m_statusMsg = (const char*)u8"ルーム一覧を取得中...";
    m_backToLobbyRequested = false;
    m_showPasswordPopup = false;
    m_targetPrivateRoomId = 0;
    m_inputPassword[0] = '\0';

    if (gameContext.networkManager.IsConnected())
    {
        gameContext.networkManager.SendRequestRoomList();
    }
}

void MultiSearchScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    gameContext.networkManager.Update();

    if (!gameContext.networkManager.IsConnected() || m_backToLobbyRequested)
    {
        sceneController.RequestSwitch(SceneId::LobbyScene);
        return;
    }

    // 3秒ごとにルーム一覧を自動更新
    float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
    m_refreshTimer += dt;
    if (m_refreshTimer >= 3.0f)
    {
        m_refreshTimer = 0.0f;
        gameContext.networkManager.SendRequestRoomList();
    }

    std::vector<uint8_t> packet;
    while (gameContext.networkManager.PopPacket(packet))
    {
        if (packet.size() < sizeof(PacketHeader))
            continue;

        PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

        // ルーム一覧の受信
        if (header->type == PacketType::SC_RoomList && packet.size() >= sizeof(MsgRoomList))
        {
            auto* listPkt = reinterpret_cast<const MsgRoomList*>(packet.data());
            m_roomList.clear();
            for (uint8_t i = 0; i < listPkt->roomCount && i < MAX_LISTED_ROOMS; ++i)
            {
                m_roomList.push_back(listPkt->rooms[i]);
            }
            m_statusMsg = m_roomList.empty() ? (const char*)u8"現在募集中のルームはありません。"
                                             : (const char*)u8"参加したいルームを選択してください。";
        }
        // 参加結果の受信
        else if (header->type == PacketType::SC_JoinRoomResult && packet.size() >= sizeof(MsgJoinRoomResult))
        {
            auto* resPkt = reinterpret_cast<const MsgJoinRoomResult*>(packet.data());
            if (resPkt->success == 0)
            {
                if (resPkt->reason == 2)
                {
                    m_statusMsg = (const char*)u8"【エラー】パスワードが一致しません！";
                }
                else
                {
                    m_statusMsg = (const char*)u8"【エラー】ルームが満員か既に解散されました。";
                }
                gameContext.networkManager.SendRequestRoomList();
            }
        }
        // マッチング成立通知（ホストが設定したモードのゲームシーンへ直接移行）
        else if (header->type == PacketType::SC_MatchStart && packet.size() >= sizeof(MsgMatchStart))
        {
            auto* matchPkt = reinterpret_cast<const MsgMatchStart*>(packet.data());
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

    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Escape))
    {
        if (m_showPasswordPopup)
        {
            m_showPasswordPopup = false;
        }
        else
        {
            sceneController.RequestSwitch(SceneId::LobbyScene);
        }
    }
}

void MultiSearchScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));
    ImGui::Begin("FullMultiSearchWindow", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    float panelW = (std::min)(860.0f, screenWidth - 60.0f);
    float panelH = (std::min)(600.0f, screenHeight - 60.0f);
    ImGui::SetCursorPos(ImVec2((screenWidth - panelW) * 0.5f, (screenHeight - panelH) * 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
    ImGui::BeginChild("MultiSearchPanel", ImVec2(panelW, panelH), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // ヘッダー行（タイトル + 右寄せボタン2つ）
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"マルチサーチ（募集中ルーム一覧）");
    ImGui::SetWindowFontScale(1.0f);

    float headerAvailW = ImGui::GetContentRegionAvail().x;
    const float btnRefreshW = 120.0f;
    const float btnBackW = 125.0f;
    const float btnGap = 10.0f;
    ImGui::SameLine(headerAvailW - (btnRefreshW + btnBackW + btnGap));
    if (ImGui::Button((const char*)u8"一覧を更新", ImVec2(btnRefreshW, 34.0f)))
    {
        gameContext.networkManager.SendRequestRoomList();
    }
    ImGui::SameLine(0.0f, btnGap);
    if (ImGui::Button((const char*)u8"ロビーへ戻る", ImVec2(btnBackW, 34.0f)))
    {
        m_backToLobbyRequested = true;
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s", m_statusMsg.c_str());
    ImGui::Spacing();

    // 残りの高さをすべてルーム一覧スクロール枠に割り当てる
    float listHeight = ImGui::GetContentRegionAvail().y;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::BeginChild("RoomListScroll", ImVec2(0.0f, listHeight), true);

    for (size_t i = 0; i < m_roomList.size(); ++i)
    {
        const auto& room = m_roomList[i];
        ImGui::PushID(static_cast<int>(room.roomId));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
        ImGui::BeginChild("RoomRow", ImVec2(0.0f, 84.0f), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        float rowAvailW = ImGui::GetContentRegionAvail().x;
        const float joinBtnW = 160.0f;
        const float joinBtnH = 44.0f;

        // 左側：ルーム情報（2行）
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Room #%u", room.roomId);
        ImGui::SameLine(120.0f);
        ImGui::Text((const char*)u8"ホスト: %s", room.hostName);

        ImGui::Spacing();

        ImGui::Text((const char*)u8"モード: %s",
                    (room.gameMode == 1) ? (const char*)u8"アドバンス" : (const char*)u8"オリジナル");
        ImGui::SameLine(230.0f);
        if (room.isPrivate)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.25f, 1.0f), (const char*)u8"[プライベート：PWあり]");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), (const char*)u8"[フリー]");
        }
        ImGui::EndGroup();

        // 右側：参加ボタン（カード内の右端に配置）
        ImGui::SameLine(rowAvailW - joinBtnW);
        ImGui::SetCursorPosY((84.0f - joinBtnH) * 0.5f);
        if (ImGui::Button((const char*)u8"このルームに参加", ImVec2(joinBtnW, joinBtnH)))
        {
            if (room.isPrivate)
            {
                m_targetPrivateRoomId = room.roomId;
                m_targetHostName = room.hostName;
                m_inputPassword[0] = '\0';
                m_showPasswordPopup = true;
            }
            else
            {
                gameContext.networkManager.SendJoinRoom(room.roomId, "");
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // プライベート部屋用 数字3桁パスワード入力ウィンドウ
    if (m_showPasswordPopup)
    {
        ImGui::OpenPopup("PrivatePasswordPopup");
    }

    if (ImGui::BeginPopupModal("PrivatePasswordPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text((const char*)u8"Room #%u（ホスト: %s）", m_targetPrivateRoomId, m_targetHostName.c_str());
        ImGui::Text((const char*)u8"3桁の数字パスワードを入力してください:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputText("##JoinPwInput", m_inputPassword, sizeof(m_inputPassword), ImGuiInputTextFlags_CharsDecimal);

        ImGui::Spacing();
        if (ImGui::Button((const char*)u8"参加する", ImVec2(110.0f, 36.0f)))
        {
            gameContext.networkManager.SendJoinRoom(m_targetPrivateRoomId, m_inputPassword);
            m_showPasswordPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button((const char*)u8"キャンセル", ImVec2(110.0f, 36.0f)))
        {
            m_showPasswordPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();

    gameContext.debugRenderer.Render(context, &gameContext.commonStates);
}