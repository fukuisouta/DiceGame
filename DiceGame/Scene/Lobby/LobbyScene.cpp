#include "pch.h"
#include "LobbyScene.h"
#include "Network/NetworkPackets.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

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

// 毎フレームのパケット受信処理・入力検知・シーン遷移管理
void LobbyScene::Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext)
{
    // ホスト側の「対戦を開始する」クリックによるモード選択シーン（SelectScene）への安全遷移
    if (m_startRequestedByHost)
    {
        m_startRequestedByHost = false;
        sceneController.RequestSwitch(SceneId::SelectScene);
        return;
    }

    if (gameContext.networkManager.IsConnected())
    {
        // 1. 通信確立直後、自身のプレイヤー名を相手へ送信（初回のみ実行）
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

        // 2. ネットワークメッセージ受信監視ループ
        std::vector<uint8_t> packet;
        while (gameContext.networkManager.PopPacket(packet))
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(packet.data());

            // 対戦相手のプレイヤー名を受信・反映
            if (header->type == PacketType::CS_PlayerInfo)
            {
                if (packet.size() >= sizeof(MsgPlayerInfo))
                {
                    MsgPlayerInfo* infoPacket = reinterpret_cast<MsgPlayerInfo*>(packet.data());
                    m_opponentName = infoPacket->playerName;
                }
            }
            // クライアント側：ホストからの「ゲーム開始」命令を受信して同期遷移
            else if (header->type == PacketType::CS_StartGame)
            {
                sceneController.RequestSwitch(SceneId::SelectScene);
                return;
            }
        }
    }
    else
    {
        // 切断状態の場合はフラグおよび相手名を初期化
        m_hasSentPlayerInfo = false;
        m_opponentName = (const char*)u8"対戦相手を待っています...";
    }

    // ESCキー押下で安全にネットワークを切断しタイトル画面へ戻る
    auto kbState = DirectX::Keyboard::Get().GetState();
    if (kbState.IsKeyDown(DirectX::Keyboard::Keys::Escape))
    {
        gameContext.networkManager.ForceDisconnect(); // ソケットの強制破棄・切断
        sceneController.RequestSwitch(SceneId::TitleScene);
        return;
    }
}

// ロビーUIおよび接続ルーム情報の2D描画処理
void LobbyScene::Render(GameContext& gameContext)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    // 1. 全画面接続設定UIのレンダリング
    m_networkUI.Render(gameContext);

    // 2. 接続確立時の「対戦部屋ステータス」およびスタートボタン描画
    if (gameContext.networkManager.IsConnected())
    {
        auto outputSize = gameContext.deviceResources.GetOutputSize();
        float screenWidth = static_cast<float>(outputSize.right - outputSize.left);
        float screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

        float matchWidth = (std::min)(680.0f, screenWidth - 80.0f);
        float matchHeight = 230.0f;

        ImGui::SetNextWindowPos(ImVec2((screenWidth - matchWidth) * 0.5f, screenHeight * 0.5f + 50.0f));
        ImGui::SetNextWindowSize(ImVec2(matchWidth, matchHeight));

        ImGui::Begin("Match Room Status", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // タイトル表示
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"対戦部屋の状況");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        // 2カラムレイアウトで双方のプレイヤー名を表示
        ImGui::Columns(2, "PlayersColumn", false);

        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            // 部屋主（Host: 1P）視点表示
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"[ プレイヤー1（あなた：部屋主） ]");
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Text("%s", m_networkUI.GetPlayerName().c_str());
            ImGui::SetWindowFontScale(1.0f);

            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.00f), (const char*)u8"[ プレイヤー2（対戦相手） ]");
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Text("%s", m_opponentName.c_str());
            ImGui::SetWindowFontScale(1.0f);
        }
        else
        {
            // ゲスト（Client: 2P）視点表示
            ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.00f), (const char*)u8"[ プレイヤー1（部屋主） ]");
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Text("%s", m_opponentName.c_str());
            ImGui::SetWindowFontScale(1.0f);

            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.00f, 1.00f), (const char*)u8"[ プレイヤー2（あなた） ]");
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Text("%s", m_networkUI.GetPlayerName().c_str());
            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ホストのみ対戦開始ボタンの操作が可能
        if (gameContext.networkManager.GetMode() == NetworkManager::Mode::Host)
        {
            if (ImGui::Button((const char*)u8"対戦を開始する", ImVec2(-1, 52)))
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
            // クライアント側は待機メッセージを表示
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.30f, 1.00f),
                               (const char*)u8"準備完了！ 部屋主がゲームを開始するのを待っています...");
        }

        ImGui::End();
    }

    Imase::DebugRenderer& debugRenderer = gameContext.debugRenderer;
    debugRenderer.Render(context, &gameContext.commonStates);
}