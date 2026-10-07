//#pragma once
//
//#include <array>
//#include <memory>
//#include <vector>
//#include <string>
//#include <format>
//#include <wrl/client.h>
//#include <Effects.h>
//#include <PrimitiveBatch.h>
//#include <VertexTypes.h>
//#include <WICTextureLoader.h>
//#include "ImaseLib/SceneManager.h"
//#include "GameContext.h"
//#include "../SceneId.h"
//#include "DiceBattleGame.h"
//#include "DiceBattleAI.h"
//#include "GridField.h"
//#include "PieceDropAnimator.h"
//#include "Framework/ObjectData.h"
//
//#include "GameLogManager.h"
//#include "SkillManager.h"
//#include "ParticleManager.h" // ★ パーティクルマネージャーの追加
//
//class GamePlayScene : public Imase::SceneBase<SceneId, GameContext>
//{
//public:
//    GamePlayScene() = default;
//    ~GamePlayScene() = default;
//
//    void OnEnter(GameContext& gameContext) override;
//    void Update(Imase::ISceneController<SceneId>& sceneController, GameContext& gameContext) override;
//    void Render(GameContext& gameContext) override;
//
//    // --- マネージャーから呼び出されるヘルパーメソッド群 ---
//    void TriggerTurnNotice(const std::wstring& text, float duration = 1.5f);
//    void ResetMoveState(); // カーソルと移動フラグのリセット
//
//    template <typename... Args> void AddLog(const wchar_t* fmt, Args&&... args)
//    {
//        if constexpr (sizeof...(Args) == 0)
//            m_logManager.AddLog(fmt);
//        else
//            m_logManager.AddLog(std::vformat(fmt, std::make_wformat_args(args...)));
//    }
//    template <typename... Args> void AddLogFormat(const wchar_t* fmt, Args&&... args)
//    {
//        AddLog(fmt, std::forward<Args>(args)...);
//    }
//
//private:
//    void LoadSceneFromJson(GameContext& gameContext, const std::string& filePath);
//    void ExecuteMove(int fromX, int fromY, int toX, int toY);
//
//private:
//    GridField m_field;
//    DiceBattleGame m_gameLogic;
//    DiceBattleAI m_ai;
//    PieceDropAnimator m_dropAnimator;
//
//    SkillManager m_skillManager;
//    ParticleManager m_particleManager; // ★ 構造体とvectorを分離して差し替え
//
//    std::vector<PlacedObject> m_placedObjects;
//    std::vector<PlacedObject> m_loadedObjects;
//
//    int m_cursorX = -1;
//    int m_cursorY = -1;
//    int m_selectedX = -1;
//    int m_selectedY = -1;
//
//    DirectX::SimpleMath::Vector3 m_cameraEye = DirectX::SimpleMath::Vector3(0.0f, 22.0f, -20.0f);
//    DirectX::SimpleMath::Vector3 m_cameraTarget = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
//    float m_cameraPitch = 48.0f;
//    float m_cameraDistance = 30.0f;
//
//    bool m_isGameOver = false;
//    PlayerType m_winnerPlayer = PlayerType::Player1;
//    int m_gameOverSelectIndex = 0;
//    RECT m_retryButtonRect = {0, 0, 0, 0};
//    RECT m_titleButtonRect = {0, 0, 0, 0};
//    bool m_isRetryHovered = false;
//    bool m_isTitleHovered = false;
//
//    PlayerType m_previousTurn = PlayerType::Player1;
//    std::wstring m_turnNoticeText = L"";
//    float m_turnNoticeTimer = 0.0f;
//    float m_turnNoticeMaxTime = 1.5f;
//
//    GameLogManager m_logManager;
//
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rollDiceButtonTexture;
//    RECT m_rollButtonRect = {0, 0, 0, 0};
//    bool m_isRollButtonHovered = false;
//
//    RECT m_btnUpRect = {0, 0, 0, 0};
//    RECT m_btnDownRect = {0, 0, 0, 0};
//    RECT m_btnLeftRect = {0, 0, 0, 0};
//    RECT m_btnRightRect = {0, 0, 0, 0};
//    bool m_isUpHovered = false;
//    bool m_isDownHovered = false;
//    bool m_isLeftHovered = false;
//    bool m_isRightHovered = false;
//
//    bool m_hasMovedThisTurn = false;
//
//    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionTexture>> m_primitiveBatch;
//    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_colorPrimitiveBatch;
//    std::unique_ptr<DirectX::BasicEffect> m_effect;
//    std::unique_ptr<DirectX::BasicEffect> m_colorEffect;
//    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 4> m_diceCountTextures;
//};