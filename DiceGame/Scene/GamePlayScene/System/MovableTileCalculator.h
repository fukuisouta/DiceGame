#pragma once

#include <vector>
#include <DirectXMath.h>
#include "../env/GridField.h"

// ============================================================================
// 現在選択中のコマ・残り歩数に応じた移動可能マスおよび予測出目の計算クラス
// ============================================================================
class MovableTileCalculator
{
public:
    static std::vector<MovableTileInfo> Calculate(int selectedX, int selectedY, int remainingSteps,
                                                  bool isWaitingForRoll, const class DiceBattleGame& gameLogic);
};