#include "pch.h"
#include "DiceBattleAI.h"
#include "DiceBattleGame.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace
{
    int PredictFaceFromCurrent(int currentFace, int dx, int dy)
    {
        if (currentFace < 1 || currentFace > 6)
            return currentFace;

        static const int rollMap[7][4] = {
            {0, 0, 0, 0}, {3, 4, 2, 5}, // 1
            {3, 4, 6, 1},               // 2
            {6, 1, 2, 5},               // 3
            {1, 6, 2, 5},               // 4
            {5, 4, 1, 6},               // 5
            {4, 3, 2, 5}                // 6
        };

        int dirIndex = -1;
        if (dx == 1)
            dirIndex = 0;
        else if (dx == -1)
            dirIndex = 1;
        else if (dy == 1)
            dirIndex = 2;
        else if (dy == -1)
            dirIndex = 3;

        if (dirIndex == -1)
            return currentFace;
        return rollMap[currentFace][dirIndex];
    }
}

void DiceBattleAI::Update(DiceBattleGame& game, float dt)
{
    if (game.IsGameOver() || game.GetCurrentTurn() != PlayerType::Player2)
    {
        m_thinkTimer = 0.0f;
        return;
    }

    if (game.IsWaitingForRoll() || game.IsAnyPieceMoving())
        return;

    m_thinkTimer += dt;
    if (m_thinkTimer < m_thinkDelay)
        return;

    m_thinkTimer = 0.0f;

    if (game.GetRemainingSteps() <= 0)
    {
        game.StartTurn();
        return;
    }

    if (!ExecuteMove(game))
    {
        game.ForceEndTurn();
    }
}

int DiceBattleAI::EvaluateBoard(const SimBoard& board)
{
    if (board.isGameOver)
    {
        // ★ 引き分け時はスコア 0（負け(-100000)より有利、勝ち(+100000)より低評価）
        if (board.isDraw)
            return 0;

        if (board.winner == PlayerType::Player2)
            return 100000;
        if (board.winner == PlayerType::Player1)
            return -100000;
    }

    if (m_ruleMode == GameRuleMode::Original)
    {
        return EvaluateOriginal(board);
    }
    else
    {
        return EvaluateAdvance(board);
    }
}

int DiceBattleAI::EvaluateOriginal(const SimBoard& board)
{
    int score = 0;
    const SimPiece* p1King = nullptr;
    const SimPiece* p2King = nullptr;

    for (const auto& piece : board.pieces)
    {
        if (!piece.isActive)
            continue;

        int pieceValue = (piece.type == PieceType::King) ? 8000 : (100 + piece.face * 30);

        if (piece.owner == PlayerType::Player2)
        {
            score += pieceValue;
            score += piece.gridY * 5;
            if (piece.type == PieceType::King)
                p2King = &piece;
        }
        else
        {
            score -= pieceValue;
            score -= (DiceBattleGame::BOARD_HEIGHT - 1 - piece.gridY) * 5;
            if (piece.type == PieceType::King)
                p1King = &piece;
        }
    }

    if (p1King)
    {
        for (const auto& piece : board.pieces)
        {
            if (piece.isActive && piece.owner == PlayerType::Player2 && piece.type == PieceType::Pawn)
            {
                int dist = std::abs(piece.gridX - p1King->gridX) + std::abs(piece.gridY - p1King->gridY);
                if (dist == 1 && piece.face == 1)
                    score += 1500;
            }
        }
    }

    if (p2King)
    {
        for (const auto& piece : board.pieces)
        {
            if (piece.isActive && piece.owner == PlayerType::Player1 && piece.type == PieceType::Pawn)
            {
                int dist = std::abs(piece.gridX - p2King->gridX) + std::abs(piece.gridY - p2King->gridY);
                if (dist == 1 && piece.face == 1)
                    score -= 4000;
            }
        }
    }

    return score;
}

int DiceBattleAI::EvaluateAdvance(const SimBoard& board)
{
    int score = EvaluateOriginal(board);

    if (board.p2Shielded)
        score += 200;
    if (board.p1Shielded)
        score -= 200;

    return score;
}

std::vector<DiceBattleAI::MoveOption> DiceBattleAI::GenerateValidMoves(const SimBoard& board, PlayerType player)
{
    std::vector<MoveOption> moves;
    const int dirX[] = {0, 0, -1, 1};
    const int dirY[] = {1, -1, 0, 0};

    bool isTargetShielded = (m_ruleMode == GameRuleMode::Advance)
                                ? ((player == PlayerType::Player2) ? board.p1Shielded : board.p2Shielded)
                                : false;

    for (const auto& piece : board.pieces)
    {
        if (!piece.isActive || piece.owner != player)
            continue;

        if (m_ruleMode == GameRuleMode::Advance && piece.isFrozen)
            continue;

        for (int i = 0; i < 4; ++i)
        {
            int dx = dirX[i];
            int dy = dirY[i];
            int targetX = piece.gridX + dx;
            int targetY = piece.gridY + dy;

            if (targetX < 0 || targetX >= DiceBattleGame::BOARD_WIDTH || targetY < 0 ||
                targetY >= DiceBattleGame::BOARD_HEIGHT)
                continue;

            auto it = std::find_if(board.pieces.begin(), board.pieces.end(), [&](const SimPiece& p)
                                   { return p.isActive && p.gridX == targetX && p.gridY == targetY; });

            const SimPiece* targetPiece = (it != board.pieces.end()) ? &(*it) : nullptr;
            if (targetPiece && targetPiece->owner == player)
                continue;

            int moveScore = 0;

            if (targetPiece && targetPiece->owner != player)
            {
                if (m_ruleMode == GameRuleMode::Advance && isTargetShielded)
                {
                    moveScore -= (piece.type == PieceType::King) ? 50000 : 3000;
                }
                else
                {
                    int myFutureFace = (piece.type == PieceType::King) ? 7 : PredictFaceFromCurrent(piece.face, dx, dy);
                    int enemyFace = targetPiece->face;

                    if (piece.type == PieceType::King && targetPiece->type == PieceType::King)
                    {
                        moveScore += 5000;
                    }
                    else if (piece.type == PieceType::King)
                    {
                        if (enemyFace == 1)
                            moveScore -= 10000;
                        else
                            moveScore += 800;
                    }
                    else if (targetPiece->type == PieceType::King)
                    {
                        if (myFutureFace == 1)
                            moveScore += 25000;
                        else
                            moveScore -= 2000;
                    }
                    else
                    {
                        if (myFutureFace > enemyFace)
                            moveScore += 1000 + (myFutureFace - enemyFace) * 150;
                        else if (myFutureFace == enemyFace)
                            moveScore -= 100;
                        else
                            moveScore -= 3000;
                    }
                }
            }
            else
            {
                if (player == PlayerType::Player2 && dy == 1)
                    moveScore += 30;
                if (player == PlayerType::Player1 && dy == -1)
                    moveScore += 30;
            }

            moves.push_back({piece.gridX, piece.gridY, targetX, targetY, moveScore});
        }
    }

    std::sort(moves.begin(), moves.end(), [](const MoveOption& a, const MoveOption& b) { return a.score > b.score; });

    return moves;
}

DiceBattleAI::SimBoard DiceBattleAI::ApplyMove(const SimBoard& board, const MoveOption& move, PlayerType player)
{
    SimBoard nextBoard = board;

    auto myIt = std::find_if(nextBoard.pieces.begin(), nextBoard.pieces.end(), [&](const SimPiece& p)
                             { return p.isActive && p.gridX == move.fromX && p.gridY == move.fromY; });

    if (myIt == nextBoard.pieces.end())
        return nextBoard;

    int dx = move.toX - move.fromX;
    int dy = move.toY - move.fromY;

    if (myIt->type != PieceType::King)
    {
        myIt->face = PredictFaceFromCurrent(myIt->face, dx, dy);
    }

    auto enemyIt = std::find_if(nextBoard.pieces.begin(), nextBoard.pieces.end(), [&](const SimPiece& p)
                                { return p.isActive && p.gridX == move.toX && p.gridY == move.toY; });

    if (enemyIt != nextBoard.pieces.end())
    {
        bool isEnemyShielded = (m_ruleMode == GameRuleMode::Advance)
                                   ? ((player == PlayerType::Player2) ? nextBoard.p1Shielded : nextBoard.p2Shielded)
                                   : false;

        if (m_ruleMode == GameRuleMode::Advance && isEnemyShielded)
        {
            myIt->isActive = false;
        }
        else if (myIt->type == PieceType::King && enemyIt->type == PieceType::King)
        {
            myIt->isActive = false;
            enemyIt->isActive = false;
        }
        else if (myIt->type == PieceType::King)
        {
            if (enemyIt->face == 1)
                myIt->isActive = false;
            else
                enemyIt->isActive = false;
        }
        else if (enemyIt->type == PieceType::King)
        {
            if (myIt->face == 1)
                enemyIt->isActive = false;
            else
                myIt->isActive = false;
        }
        else
        {
            if (myIt->face > enemyIt->face)
                enemyIt->isActive = false;
            else if (myIt->face < enemyIt->face)
                myIt->isActive = false;
            else
            {
                myIt->isActive = false;
                enemyIt->isActive = false;
            }
        }
    }

    if (myIt->isActive)
    {
        myIt->gridX = move.toX;
        myIt->gridY = move.toY;
    }

    nextBoard.remainingSteps--;
    if (nextBoard.remainingSteps <= 0)
    {
        nextBoard.currentTurn =
            (nextBoard.currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
        nextBoard.remainingSteps = 2;
    }

    bool p1KingAlive =
        std::any_of(nextBoard.pieces.begin(), nextBoard.pieces.end(), [](const SimPiece& p)
                    { return p.isActive && p.owner == PlayerType::Player1 && p.type == PieceType::King; });
    bool p2KingAlive =
        std::any_of(nextBoard.pieces.begin(), nextBoard.pieces.end(), [](const SimPiece& p)
                    { return p.isActive && p.owner == PlayerType::Player2 && p.type == PieceType::King; });

    // ★ 勝敗・引き分け（Draw）判定の適用
    if (!p1KingAlive || !p2KingAlive)
    {
        nextBoard.isGameOver = true;
        if (!p1KingAlive && !p2KingAlive)
        {
            nextBoard.isDraw = true;
        }
        else if (p1KingAlive)
        {
            nextBoard.isDraw = false;
            nextBoard.winner = PlayerType::Player1;
        }
        else if (p2KingAlive)
        {
            nextBoard.isDraw = false;
            nextBoard.winner = PlayerType::Player2;
        }
    }

    return nextBoard;
}

int DiceBattleAI::AlphaBeta(SimBoard board, int depth, int alpha, int beta, bool isMaximizing)
{
    if (depth == 0 || board.isGameOver)
    {
        return EvaluateBoard(board);
    }

    PlayerType currentPlayer = board.currentTurn;
    auto validMoves = GenerateValidMoves(board, currentPlayer);

    if (validMoves.empty())
    {
        return EvaluateBoard(board);
    }

    if (isMaximizing)
    {
        int maxEval = INT_MIN;
        for (const auto& move : validMoves)
        {
            SimBoard nextBoard = ApplyMove(board, move, currentPlayer);
            bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);
            int eval = move.score + AlphaBeta(nextBoard, depth - 1, alpha, beta, nextIsMax);
            maxEval = (std::max)(maxEval, eval);
            alpha = (std::max)(alpha, eval);
            if (beta <= alpha)
                break;
        }
        return maxEval;
    }
    else
    {
        int minEval = INT_MAX;
        for (const auto& move : validMoves)
        {
            SimBoard nextBoard = ApplyMove(board, move, currentPlayer);
            bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);
            int eval = -move.score + AlphaBeta(nextBoard, depth - 1, alpha, beta, nextIsMax);
            minEval = (std::min)(minEval, eval);
            beta = (std::min)(beta, eval);
            if (beta <= alpha)
                break;
        }
        return minEval;
    }
}

bool DiceBattleAI::ExecuteMove(DiceBattleGame& game)
{
    SimBoard currentBoard;
    const auto& pieces = game.GetPieces();

    for (const auto& piece : pieces)
    {
        if (!piece.isActive)
            continue;

        SimPiece sp;
        sp.gridX = piece.gridX;
        sp.gridY = piece.gridY;
        sp.owner = piece.owner;
        sp.type = piece.type;
        sp.face = (piece.type == PieceType::King) ? 7 : (piece.diceInstance ? piece.diceInstance->GetUpwardFace() : 1);
        sp.isActive = true;

        currentBoard.pieces.push_back(sp);
    }

    currentBoard.currentTurn = PlayerType::Player2;
    currentBoard.remainingSteps = game.GetRemainingSteps();

    if (m_ruleMode == GameRuleMode::Advance)
    {
        currentBoard.p1Shielded = game.IsPlayerShielded(PlayerType::Player1);
        currentBoard.p2Shielded = game.IsPlayerShielded(PlayerType::Player2);
    }

    auto validMoves = GenerateValidMoves(currentBoard, PlayerType::Player2);
    if (validMoves.empty())
        return false;

    int bestScore = INT_MIN;
    std::vector<MoveOption> bestMoves;

    for (auto& move : validMoves)
    {
        SimBoard nextBoard = ApplyMove(currentBoard, move, PlayerType::Player2);
        bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);

        int totalScore = move.score + AlphaBeta(nextBoard, m_searchDepth - 1, INT_MIN, INT_MAX, nextIsMax);

        if (totalScore > bestScore)
        {
            bestScore = totalScore;
            bestMoves.clear();
            bestMoves.push_back(move);
        }
        else if (totalScore == bestScore)
        {
            bestMoves.push_back(move);
        }
    }

    std::uniform_int_distribution<size_t> dist(0, bestMoves.size() - 1);
    MoveOption chosen = bestMoves[dist(m_rng)];

    return game.MovePiece(chosen.fromX, chosen.fromY, chosen.toX, chosen.toY);
}