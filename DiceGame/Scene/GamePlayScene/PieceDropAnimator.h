#pragma once

#include <d3d11.h>
#include <SimpleMath.h>
#include <vector>
#include <random>
#include <memory>
#include "Dice.h"

struct DropPieceState
{
    std::shared_ptr<Dice> diceInstance;
    DirectX::SimpleMath::Vector3 startPos;
    DirectX::SimpleMath::Vector3 targetPos;
    DirectX::SimpleMath::Quaternion targetRot;
    DirectX::SimpleMath::Vector3 randomRotAxis;

    float progress = 0.0f;
    float duration = 1.2f;
    float delay = 0.0f;
};

class PieceDropAnimator
{
public:
    PieceDropAnimator() = default;
    ~PieceDropAnimator() = default;

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
        state.startPos = targetPos + DirectX::SimpleMath::Vector3(0.0f, dropHeight, 0.0f);
        state.delay = delayDist(rng);

        DirectX::SimpleMath::Vector3 axis(rotDist(rng), rotDist(rng), rotDist(rng));
        axis.Normalize();
        state.randomRotAxis = axis;

        int rotXSteps = std::uniform_int_distribution<int>(0, 3)(rng);
        int rotYSteps = std::uniform_int_distribution<int>(0, 3)(rng);
        state.targetRot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(rotYSteps * DirectX::XM_PIDIV2,
                                                                                  rotXSteps * DirectX::XM_PIDIV2, 0.0f);

        m_dropPieces.push_back(state);
    }

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

    bool IsFinished() const
    {
        for (const auto& piece : m_dropPieces)
        {
            if (piece.progress < 1.0f)
                return false;
        }
        return true;
    }

    void Clear()
    {
        m_dropPieces.clear();
    }

private:
    std::vector<DropPieceState> m_dropPieces;
};