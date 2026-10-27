#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <vector>
#include <random>
#include <memory>
#include "Dice/Dice.h"

// 落下アニメーション中の個々のコマ状態構造体
struct DropPieceState
{
    std::shared_ptr<Dice> diceInstance;         // アニメーション対象のダイスモデル
    DirectX::SimpleMath::Vector3 startPos;      // 落下開始（空中）の初期位置
    DirectX::SimpleMath::Vector3 targetPos;     // 着地ターゲット位置
    DirectX::SimpleMath::Quaternion targetRot;  // 着地時の目標回転姿勢
    DirectX::SimpleMath::Vector3 randomRotAxis; // 落下中のスピン回転軸

    float progress = 0.0f; // アニメーション進行度 (0.0f ～ 1.0f)
    float duration = 1.2f; // 落下にかかる時間 (秒)
    float delay = 0.0f;    // 落下開始までのディレイ（時間差バラつき）
};

// ============================================================================
// ゲーム開始時にコマが上空から回転しながら盤面へ着地する演出アニメーター
// ============================================================================
class PieceDropAnimator
{
public:
    PieceDropAnimator() = default;
    ~PieceDropAnimator() = default;

    // アニメーション対象コマの追加
    void AddPiece(std::shared_ptr<Dice> dice, const DirectX::SimpleMath::Vector3& targetPos, float dropHeight = 18.0f)
    {
        if (!dice)
            return;

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> rotDist(0.0f, DirectX::XM_2PI);
        std::uniform_real_distribution<float> delayDist(0.0f, 0.3f);

        DropPieceState state;
        state.diceInstance = dice;
        state.targetPos = targetPos;
        state.startPos = targetPos + DirectX::SimpleMath::Vector3(0.0f, dropHeight, 0.0f); // 高所から落下[cite: 59]
        state.delay = delayDist(rng);

        // ランダムな回転軸の算出
        DirectX::SimpleMath::Vector3 axis(rotDist(rng), rotDist(rng), rotDist(rng));
        axis.Normalize();
        state.randomRotAxis = axis;

        // 着地時に90度単位に綺麗に整列する着地目標角度の設定
        int rotXSteps = std::uniform_int_distribution<int>(0, 3)(rng);
        int rotYSteps = std::uniform_int_distribution<int>(0, 3)(rng);
        state.targetRot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(rotYSteps * DirectX::XM_PIDIV2,
                                                                                  rotXSteps * DirectX::XM_PIDIV2, 0.0f);

        m_dropPieces.push_back(state);
    }

    // 毎フレームのアニメーション・位置・スピン回転更新
    void Update(float dt)
    {
        for (auto& piece : m_dropPieces)
        {
            // 開始前のディレイ待ち
            if (piece.delay > 0.0f)
            {
                piece.delay -= dt;
                if (piece.diceInstance)
                {
                    piece.diceInstance->SetPosition(piece.startPos);
                }
                continue;
            }

            // 落下・スピン補間アニメーション
            if (piece.progress < 1.0f)
            {
                piece.progress += dt / piece.duration;
                if (piece.progress > 1.0f)
                    piece.progress = 1.0f;

                float t = piece.progress;

                // イージング関数（Cubic Out Ease）で滑らかに着地[cite: 59]
                float easeY = 1.0f - std::pow(1.0f - t, 3.0f);
                DirectX::SimpleMath::Vector3 currentPos =
                    DirectX::SimpleMath::Vector3::Lerp(piece.startPos, piece.targetPos, easeY);

                // 落下中のスピン回転量計算および球面線形補間 (Slerp)[cite: 59]
                float spinAngle = (1.0f - t) * DirectX::XM_2PI * 4.0f;
                DirectX::SimpleMath::Quaternion spinRot =
                    DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(piece.randomRotAxis, spinAngle);
                DirectX::SimpleMath::Quaternion currentRot =
                    DirectX::SimpleMath::Quaternion::Slerp(spinRot * piece.targetRot, piece.targetRot, t);

                if (piece.diceInstance)
                {
                    piece.diceInstance->SetPosition(currentPos);
                    piece.diceInstance->SetRotation(currentRot);
                }
            }
        }
    }

    // 全コマの落下アニメーション完了判定
    bool IsFinished() const
    {
        for (const auto& piece : m_dropPieces)
        {
            if (piece.progress < 1.0f)
                return false;
        }
        return true;
    }

    // アニメーション状態のクリア
    void Clear()
    {
        m_dropPieces.clear();
    }

private:
    std::vector<DropPieceState> m_dropPieces; // 登録されている全落下面リスト
};