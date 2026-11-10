#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <vector>
#include <random>
#include <memory>
#include <cmath>
#include "Dice/Dice.h"

// 落下演出用スタイル定数定義
namespace DropAnimatorConstants
{
    constexpr float DEFAULT_DROP_HEIGHT = 18.0f; // 標準の落下高度
    constexpr float DEFAULT_DURATION = 1.2f;     // 標準の落下所要時間 (秒)
    constexpr float DELAY_MAX = 0.3f;            // 最大開始ディレイ時間 (秒)
    constexpr float SPIN_TURNS = 4.0f;           // 落下中の回転数
    constexpr int SNAP_STEPS_MAX = 3;            // スナップ角度ステップの最大値
}

// 落下アニメーション中の個々のコマ状態構造体
struct DropPieceState
{
    std::shared_ptr<Dice> diceInstance;         // アニメーション対象のダイスモデル
    DirectX::SimpleMath::Vector3 startPos;      // 落下開始（空中）の初期位置
    DirectX::SimpleMath::Vector3 targetPos;     // 着地ターゲット位置
    DirectX::SimpleMath::Quaternion targetRot;  // 着地時の目標回転姿勢
    DirectX::SimpleMath::Vector3 randomRotAxis; // 落下中のスピン回転軸

    float progress = 0.0f;                                    // アニメーション進行度 (0.0f ～ 1.0f)
    float duration = DropAnimatorConstants::DEFAULT_DURATION; // 落下にかかる時間 (秒)
    float delay = 0.0f;                                       // 落下開始までのディレイ（時間差バラつき）
};

// ゲーム開始時にコマが上空から回転しながら盤面へ着地する演出アニメーター
class PieceDropAnimator
{
public:
    PieceDropAnimator() = default;
    ~PieceDropAnimator() = default;

    // アニメーション対象コマを追加登録する関数
    // 引数 dice: ダイスモデルポインタ, targetPos: 着地目標座標, dropHeight: 落下高度
    void AddPiece(std::shared_ptr<Dice> dice, const DirectX::SimpleMath::Vector3& targetPos,
                  float dropHeight = DropAnimatorConstants::DEFAULT_DROP_HEIGHT)
    {
        if (!dice)
            return;

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> rotDist(0.0f, DirectX::XM_2PI);
        std::uniform_real_distribution<float> delayDist(0.0f, DropAnimatorConstants::DELAY_MAX);

        DropPieceState state;
        state.diceInstance = dice;
        state.targetPos = targetPos;
        state.startPos = targetPos + DirectX::SimpleMath::Vector3(0.0f, dropHeight, 0.0f);
        state.delay = delayDist(rng);

        DirectX::SimpleMath::Vector3 axis(rotDist(rng), rotDist(rng), rotDist(rng));
        axis.Normalize();
        state.randomRotAxis = axis;

        int rotXSteps = std::uniform_int_distribution<int>(0, DropAnimatorConstants::SNAP_STEPS_MAX)(rng);
        int rotYSteps = std::uniform_int_distribution<int>(0, DropAnimatorConstants::SNAP_STEPS_MAX)(rng);
        state.targetRot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(rotYSteps * DirectX::XM_PIDIV2,
                                                                                  rotXSteps * DirectX::XM_PIDIV2, 0.0f);

        m_dropPieces.push_back(state);
    }

    // 毎フレームのアニメーション・位置・スピン回転を更新する関数
    // 引数 dt: デルタタイム
    void Update(float dt)
    {
        for (auto& piece : m_dropPieces)
        {
            if (piece.delay > 0.0f)
            {
                piece.delay -= dt;
                if (piece.diceInstance)
                {
                    piece.diceInstance->SetPosition(piece.startPos);
                }
                continue;
            }

            if (piece.progress < 1.0f)
            {
                piece.progress += dt / piece.duration;
                if (piece.progress > 1.0f)
                    piece.progress = 1.0f;

                float t = piece.progress;

                float easeY = 1.0f - std::pow(1.0f - t, 3.0f);
                DirectX::SimpleMath::Vector3 currentPos =
                    DirectX::SimpleMath::Vector3::Lerp(piece.startPos, piece.targetPos, easeY);

                float spinAngle = (1.0f - t) * DirectX::XM_2PI * DropAnimatorConstants::SPIN_TURNS;
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

    // 全コマの落下アニメーションが完了したかを取得する関数
    // 戻り値: 全て完了していれば true
    bool IsFinished() const
    {
        for (const auto& piece : m_dropPieces)
        {
            if (piece.progress < 1.0f)
                return false;
        }
        return true;
    }

    // アニメーション状態をクリアリセットする関数
    void Clear()
    {
        m_dropPieces.clear();
    }

private:
    std::vector<DropPieceState> m_dropPieces; // 登録中の全落下面リスト
};