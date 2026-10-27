#pragma once

#include <wrl/client.h>
#include <Effects.h>
#include <PrimitiveBatch.h>
#include <VertexTypes.h>
#include <memory>
#include <vector>
#include <string>

#include "ImaseLib/SceneManager.h"
#include "GameContext.h"
#include "../SceneId.h"
#include "Network/NetworkLobbyUI.h"
#include "Framework/SoundManager.h"

// ============================================================================
// オンライン対戦用ネットワークロビーシーン
// 接続設定・ホスト/クライアント待機・プレイヤー名交換・対戦開始要求を管理します
// ============================================================================
class LobbyScene : public Imase::SceneBase<SceneId, GameContext>
{
public:
    LobbyScene() = default;
    ~LobbyScene() override = default;

    // ロビーシーン移行時の初期化（ネットワークUI活性化等）
    void OnEnter(GameContext& gameContext) override;
    // 毎フレームのロジック・パケット受信監視・キー入力更新
    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
    // ネットワークUIおよび対戦部屋ステータスのレンダリング
    void Render(GameContext& gameContext) override;

private:
    NetworkLobbyUI m_networkUI; // ネットワーク接続設定用ImGui UI

    std::string m_opponentName = "Waiting..."; // 接続相手のプレイヤー名
    bool m_hasSentPlayerInfo = false;          // 自身のプレイヤー名パケット送信済みフラグ
    bool m_startRequestedByHost = false;       // ホストが対戦開始ボタンを押した際の遷移要求フラグ
};