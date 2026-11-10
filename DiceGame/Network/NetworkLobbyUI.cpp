#include "pch.h"
#include "NetworkLobbyUI.h"
#include "GameContext.h"
#include "Network/NetworkManager.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace LobbyStyleConstants
{
    constexpr float ROUNDING_ZERO = 0.0f;
    constexpr float ROUNDING_FRAME = 6.0f;
    constexpr float BORDER_SIZE_NONE = 0.0f;
    constexpr float BORDER_SIZE_THIN = 1.0f;

    constexpr float PADDING_WINDOW_X = 20.0f;
    constexpr float PADDING_WINDOW_Y = 20.0f;
    constexpr float PADDING_FRAME_X = 12.0f;
    constexpr float PADDING_FRAME_Y = 8.0f;
    constexpr float SPACING_ITEM_X = 12.0f;
    constexpr float SPACING_ITEM_Y = 10.0f;

    constexpr float PANEL_MAX_WIDTH = 660.0f;
    constexpr float PANEL_MAX_HEIGHT = 560.0f;
    constexpr float PANEL_MARGIN = 40.0f;
    constexpr float HALF_CENTER_RATIO = 0.5f;

    constexpr float FONT_SCALE_HEADER = 1.15f;
    constexpr float FONT_SCALE_NORMAL = 1.0f;

    constexpr float BUTTON_EXIT_HEIGHT = 38.0f;
    constexpr float BUTTON_ACTION_HEIGHT = 44.0f;
    constexpr float RADIO_POS_X = 280.0f;
}

// ロビーUI初期化
void NetworkLobbyUI::Initialize()
{
    std::string myIP = GetLocalIPAddress();
    if (!myIP.empty())
    {
        m_myIPAddress = myIP;
        strcpy_s(m_targetIpBuffer, sizeof(m_targetIpBuffer), myIP.c_str());
    }
}

// UI描画処理
void NetworkLobbyUI::Render(GameContext& gameContext)
{
    if (!m_isVisible)
        return;

    // 1. スタイル設定
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = LobbyStyleConstants::ROUNDING_ZERO;
    style.FrameRounding = LobbyStyleConstants::ROUNDING_FRAME;
    style.PopupRounding = LobbyStyleConstants::ROUNDING_FRAME;
    style.WindowBorderSize = LobbyStyleConstants::BORDER_SIZE_NONE;
    style.FrameBorderSize = LobbyStyleConstants::BORDER_SIZE_THIN;

    style.WindowPadding = ImVec2(LobbyStyleConstants::PADDING_WINDOW_X, LobbyStyleConstants::PADDING_WINDOW_Y);
    style.FramePadding = ImVec2(LobbyStyleConstants::PADDING_FRAME_X, LobbyStyleConstants::PADDING_FRAME_Y);
    style.ItemSpacing = ImVec2(LobbyStyleConstants::SPACING_ITEM_X, LobbyStyleConstants::SPACING_ITEM_Y);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.58f, 0.68f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.08f, 0.14f, 0.85f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.13f, 0.22f, 0.75f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.55f, 0.85f, 0.60f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.18f, 0.28f, 0.90f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.28f, 0.42f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.35f, 0.52f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.42f, 0.72f, 0.90f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.62f, 0.95f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.32f, 0.58f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.45f, 0.70f, 0.50f);

    // 2. 全画面固定背景ウィンドウ
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("FullLobbyWindow", nullptr, flags);

    // 3. 中央パネル
    float panelWidth =
        (std::min)(LobbyStyleConstants::PANEL_MAX_WIDTH, screenWidth - LobbyStyleConstants::PANEL_MARGIN);
    float panelHeight =
        (std::min)(LobbyStyleConstants::PANEL_MAX_HEIGHT, screenHeight - LobbyStyleConstants::PANEL_MARGIN);

    ImGui::SetCursorPos(ImVec2((screenWidth - panelWidth) * LobbyStyleConstants::HALF_CENTER_RATIO,
                               (screenHeight - panelHeight) * LobbyStyleConstants::HALF_CENTER_RATIO));

    ImGui::BeginChild("LobbyMainPanel", ImVec2(panelWidth, panelHeight), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_HEADER);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"オンライン対戦ロビー");
    ImGui::SetWindowFontScale(LobbyStyleConstants::FONT_SCALE_NORMAL);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"プレイヤー名設定");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##PlayerName", (const char*)u8"名前を入力してください...", m_playerNameBuffer,
                                 sizeof(m_playerNameBuffer)))
    {
        m_playerNameStr = m_playerNameBuffer;
    }

    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"ネットワーク情報");
    ImGui::Text((const char*)u8"あなたのIPアドレス (相手に教える番号): ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.30f, 1.00f), "%s", m_myIPAddress.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (gameContext.networkManager.IsConnected())
    {
        ImGui::TextColored(ImVec4(0.20f, 1.00f, 0.40f, 1.00f), (const char*)u8"[ 状態：接続中 ]");
        ImGui::Spacing();

        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            ImGui::TextDisabled((const char*)u8"役割：部屋主 (プレイヤー1) - 相手の参加を待っています。");
        }
        else
        {
            ImGui::TextDisabled((const char*)u8"役割：参加者 (プレイヤー2) - 部屋主が開始するのを待っています...");
        }

        ImGui::Spacing();

        if (ImGui::Button((const char*)u8"部屋を退出する", ImVec2(-1.0f, LobbyStyleConstants::BUTTON_EXIT_HEIGHT)))
        {
            gameContext.networkManager.ForceDisconnect();
            m_statusMsg = (const char*)u8"部屋から退出しました。";
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"対戦モード選択");
        ImGui::RadioButton((const char*)u8"部屋を作る (ホスト)", &m_selectedModeIndex,
                           LobbyUIConstants::MODE_HOST_INDEX);
        ImGui::SameLine(LobbyStyleConstants::RADIO_POS_X);
        ImGui::RadioButton((const char*)u8"部屋に入る (参加)", &m_selectedModeIndex,
                           LobbyUIConstants::MODE_CLIENT_INDEX);

        ImGui::Spacing();

        if (m_selectedModeIndex == LobbyUIConstants::MODE_HOST_INDEX)
        {
            ImGui::TextDisabled((const char*)u8"自分で対戦部屋を作成し、相手が接続してくるのを待ちます。");
            ImGui::Spacing();

            if (ImGui::Button((const char*)u8"部屋を作って待機する",
                              ImVec2(-1.0f, LobbyStyleConstants::BUTTON_ACTION_HEIGHT)))
            {
                if (gameContext.networkManager.StartHost(LobbyUIConstants::DEFAULT_PORT))
                {
                    m_statusMsg = (const char*)u8"部屋を作成しました！ 相手の接続を待っています...";
                }
                else
                {
                    m_statusMsg = (const char*)u8"部屋の作成に失敗しました。";
                }
            }
        }
        else
        {
            ImGui::TextDisabled((const char*)u8"教えてもらった部屋主のIPアドレスを入力してください:");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##HostIP", (const char*)u8"相手のIPアドレス (例: 192.168.x.x)", m_targetIpBuffer,
                                     sizeof(m_targetIpBuffer));

            ImGui::Spacing();

            if (ImGui::Button((const char*)u8"相手の部屋に入る",
                              ImVec2(-1.0f, LobbyStyleConstants::BUTTON_ACTION_HEIGHT)))
            {
                if (gameContext.networkManager.ConnectToHost(m_targetIpBuffer, LobbyUIConstants::DEFAULT_PORT))
                {
                    m_statusMsg = (const char*)u8"部屋に接続しました！";
                }
                else
                {
                    m_statusMsg = (const char*)u8"接続に失敗しました。IPアドレスを確認してください。";
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled((const char*)u8"状態: %s", m_statusMsg.c_str());
    }

    ImGui::EndChild();
    ImGui::End();
}