#include "pch.h"
#include "NetworkUI.h"
#include "imgui.h"
#include <cstring>

// 初期化：自身のローカルIPアドレスを入力バッファの初期値として設定
void NetworkUI::Initialize()
{
    std::string myIP = GetLocalIPAddress();
    if (!myIP.empty())
    {
        strcpy_s(m_ipBuffer, sizeof(m_ipBuffer), myIP.c_str());
    }
}

// デバッグ・設定用の簡略ネットワークUI描画
void NetworkUI::RenderUI(NetworkManager& networkManager)
{
    if (!m_isVisible)
        return;

    // オンライン対戦設定ウィンドウを表示
    ImGui::Begin("オンライン対戦設定", &m_isVisible, ImGuiWindowFlags_AlwaysAutoResize);

    // 現在のネットワーク接続状態をメッセージへ反映
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

    // 1. 部屋作成（ホスト起動）ボタン領域
    ImGui::Text("【自分で部屋を作る場合】");
    if (ImGui::Button("部屋を作って待機する", ImVec2(280, 40)))
    {
        if (networkManager.StartHost(12345))
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

    // 2. IP入力ボックス & クライアント接続ボタン領域
    ImGui::Text("【友達の部屋に入る場合】");
    ImGui::InputText("相手のIPアドレス", m_ipBuffer, sizeof(m_ipBuffer));

    if (ImGui::Button("相手の部屋に入る", ImVec2(280, 40)))
    {
        if (networkManager.ConnectToHost(m_ipBuffer, 12345))
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