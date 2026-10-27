#pragma once

#include "Dice.h"

// ============================================================================
// 基底クラス Dice を継承し、移動歩数（1～3）専用のロジックを持つダイスクラス
// ダイスの6面に対して 1, 2, 3 の値が2面ずつ（対面同士）割り当てられています
// ============================================================================
class StepDice : public Dice
{
public:
    StepDice() = default;
    ~StepDice() = default;

    // ダイスの現在の回転状態（クォータニオン）から、真上（ワールド空間+Y）を向いている面の歩数（1〜3）を判定して返します
    int GetStepValue() const
    {
        // 各面のローカル方向ベクトルと、それに対応する歩数値（1〜3）のマッピング定義です
        struct FaceStep
        {
            int stepValue;                         // 各面に割り当てられた移動歩数 (1~3)
            DirectX::SimpleMath::Vector3 localDir; // ダイス中心から各面へのローカル法線ベクトル
        };

        // サイコロの対面同士に同じ値を割り当て、6面全体で 1～3 が2面ずつ存在するように設定しています
        static const FaceStep faces[] = {
            {3, DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f)},  // 上面（+Y方向）
            {3, DirectX::SimpleMath::Vector3(0.0f, -1.0f, 0.0f)}, // 下面（-Y方向）
            {2, DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f)},  // 前面（+Z方向）
            {2, DirectX::SimpleMath::Vector3(0.0f, 0.0f, -1.0f)}, // 後面（-Z方向）
            {1, DirectX::SimpleMath::Vector3(1.0f, 0.0f, 0.0f)},  // 右面（+X方向）
            {1, DirectX::SimpleMath::Vector3(-1.0f, 0.0f, 0.0f)}, // 左面（-X方向）
        };

        DirectX::SimpleMath::Vector3 worldUp = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);

        // ダイスの現在の回転姿勢の逆クォータニオンを適用し、ワールド空間の上方向ベクトルをダイスのローカル座標系へ変換します
        DirectX::SimpleMath::Quaternion invRot;
        GetRotation().Inverse(invRot);
        DirectX::SimpleMath::Vector3 localUp = DirectX::SimpleMath::Vector3::Transform(worldUp, invRot);

        float maxDot = -100.0f;
        int bestStep = 1;

        // ローカル上方向ベクトルと各面の方向ベクトルの内積（Dot）を計算し、最も向きの近い（内積最大）面の歩数を判定します
        for (const auto& face : faces)
        {
            float dot = face.localDir.Dot(localUp);
            if (dot > maxDot)
            {
                maxDot = dot;
                bestStep = face.stepValue;
            }
        }

        return bestStep;
    }
};