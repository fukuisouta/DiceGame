#include "pch.h"
#include "MovableTileCalculator.h"
#include "DiceBattleGame.h"
#include "Dice/Dice.h"
#include <queue>
#include <map>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 移動可能マスおよびそのマスへ移動した際の上面出目を幅優先探索（BFS）で計算
std::vector<MovableTileInfo> MovableTileCalculator::Calculate(int selectedX, int selectedY, int remainingSteps,
                                                              bool isWaitingForRoll, const DiceBattleGame& gameLogic)
{
    std::vector<MovableTileInfo> movableTiles;

    if (selectedX < 0 || selectedY < 0 || isWaitingForRoll)
    {
        return movableTiles;
    }

    const auto* piece = gameLogic.GetPieceAt(selectedX, selectedY);
    if (!piece || !piece->isActive || piece->owner != gameLogic.GetCurrentTurn())
    {
        return movableTiles;
    }

    // --- 王（King）の特殊移動計算 ---
    if (piece->type == PieceType::King)
    {
        int destroyedCount = 0;
        for (const auto& p : gameLogic.GetPieces())
        {
            if (!p.isActive)
                destroyedCount++;
        }

        // 敵味方の撃破数に応じて移動可能な縦行エリアが拡張
        int allowedRows = std::min(DiceBattleGame::BOARD_HEIGHT, 2 + destroyedCount);

        int minY = 0;
        int maxY = DiceBattleGame::BOARD_HEIGHT - 1;

        if (piece->owner == PlayerType::Player1)
        {
            // Player1: 盤面下側のエリア
            minY = std::max(0, DiceBattleGame::BOARD_HEIGHT - allowedRows);
            maxY = DiceBattleGame::BOARD_HEIGHT - 1;
        }
        else
        {
            // Player2: 盤面上側のエリア
            minY = 0;
            maxY = std::min(DiceBattleGame::BOARD_HEIGHT - 1, allowedRows - 1);
        }

        for (int ny = minY; ny <= maxY; ++ny)
        {
            for (int nx = 0; nx < DiceBattleGame::BOARD_WIDTH; ++nx)
            {
                if (nx == selectedX && ny == selectedY)
                    continue;

                const auto* targetPiece = gameLogic.GetPieceAt(nx, ny);

                if (!targetPiece || targetPiece->owner != piece->owner)
                {
                    MovableTileInfo info;
                    info.gridX = nx;
                    info.gridY = ny;
                    info.expectedFace = 0; // 王は出目数字なし
                    movableTiles.push_back(info);
                }
            }
        }

        return movableTiles;
    }
    // --- 通常兵隊コマ（ダイス）の探索計算 ---
    else if (remainingSteps > 0)
    {
        static const int dx[] = {0, 0, -1, 1};
        static const int dy[] = {-1, 1, 0, 0};

        struct SearchNode
        {
            int x;
            int y;
            Quaternion rot;
            int stepsLeft;
            int distance;
        };

        std::map<std::pair<int, int>, MovableTileInfo> reachedTiles;
        std::map<std::pair<int, int>, int> minStepsLeftMap;
        std::queue<SearchNode> q;

        Quaternion initRot = piece->diceInstance ? piece->diceInstance->GetRotation() : Quaternion::Identity;
        q.push({selectedX, selectedY, initRot, remainingSteps, 0});
        minStepsLeftMap[{selectedX, selectedY}] = remainingSteps;

        while (!q.empty())
        {
            SearchNode curr = q.front();
            q.pop();

            if (curr.distance > 0)
            {
                std::pair<int, int> key = {curr.x, curr.y};
                if (reachedTiles.find(key) == reachedTiles.end())
                {
                    int faceToDisplay = 0;
                    if (curr.distance == 1)
                    {
                        faceToDisplay = piece->diceInstance ? piece->diceInstance->GetTopFaceFromRotation(curr.rot)
                                                            : gameLogic.GetPieceTopFace(piece);
                    }

                    reachedTiles[key] = {curr.x, curr.y, faceToDisplay};
                }
            }

            if (curr.stepsLeft == 0)
            {
                continue;
            }

            for (int i = 0; i < 4; ++i)
            {
                int nx = curr.x + dx[i];
                int ny = curr.y + dy[i];

                if (nx < 0 || nx >= DiceBattleGame::BOARD_WIDTH || ny < 0 || ny >= DiceBattleGame::BOARD_HEIGHT)
                    continue;

                const auto* targetPiece = gameLogic.GetPieceAt(nx, ny);
                if (targetPiece && targetPiece->owner == piece->owner)
                    continue;

                std::pair<int, int> nextKey = {nx, ny};
                int nextStepsLeft = curr.stepsLeft - 1;
                if (minStepsLeftMap.count(nextKey) && minStepsLeftMap[nextKey] >= nextStepsLeft)
                {
                    continue;
                }
                minStepsLeftMap[nextKey] = nextStepsLeft;

                Quaternion deltaRot = Quaternion::Identity;
                if (dx[i] == 1)
                {
                    deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -XM_PIDIV2);
                }
                else if (dx[i] == -1)
                {
                    deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, XM_PIDIV2);
                }
                else if (dy[i] == 1)
                {
                    deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitX, -XM_PIDIV2);
                }
                else if (dy[i] == -1)
                {
                    deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitX, XM_PIDIV2);
                }

                Quaternion nextRot = curr.rot * deltaRot;
                nextRot.Normalize();

                if (targetPiece && targetPiece->owner != piece->owner)
                {
                    int faceToDisplay = 0;
                    if (curr.distance + 1 == 1)
                    {
                        faceToDisplay = piece->diceInstance ? piece->diceInstance->GetTopFaceFromRotation(nextRot)
                                                            : gameLogic.GetPieceTopFace(piece);
                    }

                    if (reachedTiles.find(nextKey) == reachedTiles.end())
                    {
                        reachedTiles[nextKey] = {nx, ny, faceToDisplay};
                    }
                }
                else
                {
                    q.push({nx, ny, nextRot, nextStepsLeft, curr.distance + 1});
                }
            }
        }

        for (const auto& pair : reachedTiles)
        {
            movableTiles.push_back(pair.second);
        }
    }

    return movableTiles;
}