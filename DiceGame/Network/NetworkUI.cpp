#include "pch.h"
#include "NetworkUI.h"
#include "imgui.h"
#include <cstring>

namespace NetworkUIStyleConstants
{
    constexpr float BUTTON_WIDTH = 280.0f; // テストUIボタン幅
    constexpr float BUTTON_HEIGHT = 40.0f; // テストUIボタン高さ
}

// 初期化処理
void NetworkUI::Initialize()
{
    std::string myIP = GetLocalIPAddress();
    if (!myIP.empty())
    {
        strcpy_s(m_ipBuffer, sizeof(m_ipBuffer), myIP.c_str());
    }
}

// テストUI描画
void NetworkUI::RenderUI(NetworkManager& networkManager)
{
    if (!m_isVisible)
        return;

    ImGui::Begin("オンライン対戦設定", &m_isVisible, ImGuiWindowFlags_AlwaysAutoResize);

    if (networkManager.IsConnected())
    {
        if (networkManager.GetMode() == NetworkManager::Mode::Host)
            m_statusMsg = "部屋主（ホスト）として接続中";
        else if (networkManager.GetMode() == NetworkManager::Mode::Client)
            m_statusMsg = "対戦相手の部屋に接続中";
    }

    ImGui::Text("現在の状態: %s", m_statusMsg.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("【自分で部屋を作る場合】");
    if (ImGui::Button("部屋を作って待機する",
                      ImVec2(NetworkUIStyleConstants::BUTTON_WIDTH, NetworkUIStyleConstants::BUTTON_HEIGHT)))
    {
        if (networkManager.StartHost(NetworkUIConstants::DEBUG_PORT))
        {
            m_statusMsg = "対戦相手の接続を待っています...";
        }
        else
        {
            m_statusMsg = "部屋の作成に失敗しました";
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("【友達の部屋に入る場合】");
    ImGui::InputText("相手のIPアドレス", m_ipBuffer, sizeof(m_ipBuffer));

    if (ImGui::Button("相手の部屋に入る",
                      ImVec2(NetworkUIStyleConstants::BUTTON_WIDTH, NetworkUIStyleConstants::BUTTON_HEIGHT)))
    {
        if (networkManager.ConnectToHost(m_ipBuffer, NetworkUIConstants::DEBUG_PORT))
        {
            m_statusMsg = "接続に成功しました！";
        }
        else
        {
            m_statusMsg = "接続に失敗しました（IPアドレスを確認してください）";
        }
    }

    ImGui::End();
}