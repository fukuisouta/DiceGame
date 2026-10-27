#include "pch.h"
#include "NetworkLobbyUI.h"
#include "GameContext.h"
#include "Network/NetworkManager.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

// ロビーUIの初期化：自PCのIPアドレスを取得して設定
void NetworkLobbyUI::Initialize()
{
    std::string myIP = GetLocalIPAddress();
    if (!myIP.empty())
    {
        m_myIPAddress = myIP;
        strcpy_s(m_targetIpBuffer, sizeof(m_targetIpBuffer), myIP.c_str());
    }
}

// オンラインロビー全画面UIの描画処理
void NetworkLobbyUI::Render(GameContext& gameContext)
{
    if (!m_isVisible)
        return;

    // ==========================================
    // 1. ImGui スタイル設定（ダークブルーテーマ）
    // ==========================================
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowPadding = ImVec2(20, 20);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(12, 10);

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

    // ==========================================
    // 2. 全画面固定背景ウィンドウの設定
    // ==========================================
    auto outputSize = gameContext.deviceResources.GetOutputSize();
    float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("FullLobbyWindow", nullptr, flags);

    // ==========================================
    // 3. 中央メインパネルの配置
    // ==========================================
    float panelWidth = (std::min)(660.0f, screenWidth - 40.0f);
    float panelHeight = (std::min)(560.0f, screenHeight - 40.0f);

    ImGui::SetCursorPos(ImVec2((screenWidth - panelWidth) * 0.5f, (screenHeight - panelHeight) * 0.5f));

    ImGui::BeginChild("LobbyMainPanel", ImVec2(panelWidth, panelHeight), true, ImGuiWindowFlags_NoScrollbar);

    // ヘッダータイトル
    ImGui::SetWindowFontScale(1.15f);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"オンライン対戦ロビー");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    // プレイヤー名入力領域
    ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"プレイヤー名設定");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##PlayerName", (const char*)u8"名前を入力してください...", m_playerNameBuffer,
                                 sizeof(m_playerNameBuffer)))
    {
        m_playerNameStr = m_playerNameBuffer;
    }

    ImGui::Spacing();

    // 自PCのネットワーク情報（IP表示）
    ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"ネットワーク情報");
    ImGui::Text((const char*)u8"あなたのIPアドレス (相手に教える番号): ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.30f, 1.00f), "%s", m_myIPAddress.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 接続状態に応じた画面分岐
    if (gameContext.networkManager.IsConnected())
    {
        // --- 接続確立中の表示 ---
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

        // 切断・退出ボタン
        if (ImGui::Button((const char*)u8"部屋を退出する", ImVec2(-1, 38)))
        {
            gameContext.networkManager.ForceDisconnect();
            m_statusMsg = (const char*)u8"部屋から退出しました。";
        }
    }
    else
    {
        // --- 未接続時のラジオボタン・接続フォーム表示 ---
        ImGui::TextColored(ImVec4(0.80f, 0.88f, 0.98f, 1.00f), (const char*)u8"対戦モード選択");
        ImGui::RadioButton((const char*)u8"部屋を作る (ホスト)", &m_selectedModeIndex, 0);
        ImGui::SameLine(280);
        ImGui::RadioButton((const char*)u8"部屋に入る (参加)", &m_selectedModeIndex, 1);

        ImGui::Spacing();

        if (m_selectedModeIndex == 0)
        {
            // ホスト起動フォーム
            ImGui::TextDisabled((const char*)u8"自分で対戦部屋を作成し、相手が接続してくるのを待ちます。");
            ImGui::Spacing();

            if (ImGui::Button((const char*)u8"部屋を作って待機する", ImVec2(-1, 44)))
            {
                if (gameContext.networkManager.StartHost(46860))
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
            // クライアント接続フォーム
            ImGui::TextDisabled((const char*)u8"教えてもらった部屋主のIPアドレスを入力してください:");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##HostIP", (const char*)u8"相手のIPアドレス (例: 192.168.x.x)", m_targetIpBuffer,
                                     sizeof(m_targetIpBuffer));

            ImGui::Spacing();

            if (ImGui::Button((const char*)u8"相手の部屋に入る", ImVec2(-1, 44)))
            {
                if (gameContext.networkManager.ConnectToHost(m_targetIpBuffer, 46860))
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