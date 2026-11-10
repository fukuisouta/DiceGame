#pragma once

#include "Dice.h"

// ============================================================================
// 移動歩数（1〜3）専用ダイスクラス
// Base: Dice
// ============================================================================
class StepDice : public Dice
{
public:
    StepDice() = default;
    ~StepDice() override = default;

    // 現在のダイス姿勢から、上面を向いている移動歩数（1～3）を判定して取得
    int GetStepValue() const
    {
        using namespace DirectX::SimpleMath;

        // 各面ごとの移動歩数とローカル方向の紐付け用構造体
        struct FaceStepInfo
        {
            int stepValue;    // 該当の面に設定されている移動歩数 (1～3)
            Vector3 localDir; // ダイス中心から各面へ向かうローカル法線ベクトル
        };

        // 対面同士に同じ値を割り当てた歩数マッピングテーブル
        static const FaceStepInfo faces[] = {
            {3, Vector3(0.0f, 1.0f, 0.0f)},  // +Y (上面: 3歩)
            {3, Vector3(0.0f, -1.0f, 0.0f)}, // -Y (下面: 3歩)
            {2, Vector3(0.0f, 0.0f, 1.0f)},  // +Z (前面: 2歩)
            {2, Vector3(0.0f, 0.0f, -1.0f)}, // -Z (後面: 2歩)
            {1, Vector3(1.0f, 0.0f, 0.0f)},  // +X (右面: 1歩)
            {1, Vector3(-1.0f, 0.0f, 0.0f)}  // -X (左面: 1歩)
        };

        const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f); // 判定の基準となるワールド空間の真上ベクトル

        // クォータニオン逆変換によりワールド上方向をダイスのローカル座標系へ変換
        Quaternion invRot;
        m_rotation.Inverse(invRot);
        Vector3 localUp = Vector3::Transform(worldUp, invRot);

        float maxDot = DiceConstants::DOT_PRODUCT_INIT_MIN; // 最適面探索用の内積最大値保持変数
        int bestStep = 1;                                   // 判定された最適な上面の歩数値 (デフォルト: 1)

        // 内積計算により最も上を向いている面の歩数を抽出
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