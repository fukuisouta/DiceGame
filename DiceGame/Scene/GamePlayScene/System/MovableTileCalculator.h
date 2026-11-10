#pragma once

#include <vector>
#include <DirectXMath.h>
#include "../env/GridField.h"

// 現在選択中のコマ・残り歩数に応じた移動可能マスおよび予測出目の計算クラス
class MovableTileCalculator
{
public:
    // 選択されたコマの移動可能マス一覧および上面出目を算出する静的関数
    // 引数 selectedX, selectedY: 選択中のコマの格子座標, remainingSteps: 残り移動歩数,
    //      isWaitingForRoll: ダイス投入演出中フラグ, gameLogic: ダイスバトルゲームロジック参照
    // 戻り値: 移動可能マス情報（MovableTileInfo）のリスト
    static std::vector<MovableTileInfo> Calculate(int selectedX, int selectedY, int remainingSteps,
                                                  bool isWaitingForRoll, const class DiceBattleGame& gameLogic);
};