#include "pch.h"
#include "NetworkLobbyUI.h"
#include "GameContext.h"
#include "imgui.h"

void NetworkLobbyUI::Render(GameContext& gameContext)
{
    if (!m_isVisible)
        return;

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 220), ImGuiCond_FirstUseEver);

    ImGui::Begin("Online Match", &m_isVisible, ImGuiWindowFlags_NoResize);

    if (gameContext.networkManager.IsConnected())
    {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[Connected]");
        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
            ImGui::Text("Role: Host (Player 1)");
        else
            ImGui::Text("Role: Client (Player 2)");

        ImGui::Spacing();
        ImGui::TextWrapped("Ready to play online!");
    }
    else
    {
        ImGui::Text("Status: %s", m_statusMsg.c_str());
        ImGui::Separator();

        // 1. ホスト開始
        if (ImGui::Button("Host Game (Port: 12345)", ImVec2(-1, 35)))
        {
            if (gameContext.networkManager.StartHost(12345))
                m_statusMsg = "Waiting for Client...";
            else
                m_statusMsg = "Failed to start Host.";
        }

        ImGui::Spacing();

        // 2. クライアント接続
        ImGui::InputText("IP", m_ipBuffer, sizeof(m_ipBuffer));
        if (ImGui::Button("Connect to Host", ImVec2(-1, 35)))
        {
            if (gameContext.networkManager.ConnectToHost(m_ipBuffer, 12345))
                m_statusMsg = "Connected!";
            else
                m_statusMsg = "Connection Failed.";
        }
    }

    ImGui::End();
}